///////////////////////////////////////////////////////////////////////////////
// FILE:          BK9130B.cpp
// PROJECT:       Micro-Manager
// SUBSYSTEM:     DeviceAdapters
//-----------------------------------------------------------------------------
// DESCRIPTION:   Device adapter implementation for BK9130B using the NI-VISA
//                drivers
//
// AUTHOR:        Scottie Alexander, scottiealexander11@gmail.com
//
// COPYRIGHT:     University of California, Davis, 2016
//
// LICENSE:       This file is distributed under the BSD license.
//                License text is included with the source distribution.
//
//                This file is distributed in the hope that it will be useful,
//                but WITHOUT ANY WARRANTY; without even the implied warranty
//                of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
//
//                IN NO EVENT SHALL THE COPYRIGHT OWNER OR
//                CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
//                INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES.

#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>
#include <iostream>

#include "ModuleInterface.h"
#include "DeviceUtils.h"

#include "visa.h"
#include "BK9130B.h"

const char* g_PSUName = "BK9130B";

const char* g_PSUDeviceIDProperty = "Device ID";

const char* g_PSUTimeoutProperty = "Timeout (ms)";

const char* g_PSULockProperty = "Lock Mode";
const char* g_PSULock_None = "None";
const char* g_PSULock_Shared = "Shared";
const char* g_PSULock_Exclusive = "Exclusive";

const char* g_PSUActiveChannelProperty = "Active Channel";
const char* g_PSUActiveChannel_CH1 = "CH1";
const char* g_PSUActiveChannel_CH2 = "CH2";
const char* g_PSUActiveChannel_CH3 = "CH3";

const char* g_PSUOutputVoltageProperty = "Output voltage (V)";

const char* g_PSUOutputCurrentProperty = "Output current (A)";

/*------------------------------------------------------------------------------
  Exported MMDevice API
------------------------------------------------------------------------------*/

/**
 * List all supported hardware devices here
 */
MODULE_API void InitializeModuleData()
{
   RegisterDevice(g_PSUName, BK9310B_DEVICE_TYPE, "BK Precision 9130B power supply");
}
/*----------------------------------------------------------------------------*/
MODULE_API MM::Device* CreateDevice(const char* deviceName)
{
   if (deviceName != 0)
   {
	   // decide which device class to create based on the deviceName parameter
	   if (strcmp(deviceName, g_PSUName) == 0)
	   {
		  // create BK9130B instance
		  return new BK9130B();
	   }
   }

   // ...supplied name not recognized
   return 0;
}
/*----------------------------------------------------------------------------*/
MODULE_API void DeleteDevice(MM::Device* pDevice)
{
   delete pDevice;
}
/*============================================================================*/
/**
* BK9130B implementation
*/
BK9130B::BK9130B() :
	dev_(),
	initialized_(false),
	busy_(false),
	timeout_(2000),
	activeChannel_(""),
	activeChannelState_(false),
	outputVoltage_(1.0),
	outputCurrent_(0.0)
{
	// call the base class method to set-up default error codes/messages
	InitializeDefaultErrorMessages();

	SetErrorText(ERR_INVALID_CHANNEL, "Invalid channel given: MUST be 1 OR 2");
	SetErrorText(ERR_INVALID_VOLTAGE, "Invalid voltage request: MUST be 0-30 V for CH1 & 2, *AND* 0-5 V for CH3");
	SetErrorText(ERR_INVALID_CURRENT, "Invalid current request: MUST be 0-3 A");
	SetErrorText(ERR_WRITE_FAILED, "Write operation failed!");
	SetErrorText(ERR_READ_FAILED, "Read operation failed!");
	SetErrorText(ERR_QUERY_FAILED, "Query operation failed!");

	// Description property
	int ret = CreateProperty(MM::g_Keyword_Description, "BK Precision 9130B power supply", MM::String, true);
	assert(ret == DEVICE_OK);

	// Device ID property
	std::vector<std::string> devIDs = dev_.findInstruments("?*");

	std::string defID;

	if (devIDs.size() > 0)
	{
		defID = devIDs[0];
	}
	else
	{
		defID = "<no devices found>";
	}

	ret = CreateProperty(g_PSUDeviceIDProperty, // property name
		defID.c_str(),							// default value
		MM::String,								// property type
		true,									// read only
		0,										// action handler
		true									// pre-init
	);
	assert(ret == DEVICE_OK);

	if (devIDs.size() > 0)
	{
		ret = SetAllowedValues(g_PSUDeviceIDProperty, devIDs);
		assert(ret == DEVICE_OK);
	}
	else
	{
		LogMessage("Failed to locate BK9130B!");
	}

	// Timeout property
	ret = CreateIntegerProperty(g_PSUTimeoutProperty, 2000, false, 0, true);
	assert(ret == DEVICE_OK);

	ret = SetPropertyLimits(g_PSUTimeoutProperty, 0, 1e6);
	assert(ret == DEVICE_OK);

	// Lock property
	ret = CreateProperty(g_PSULockProperty, g_PSULock_None, MM::String, false, 0, true);
	assert(ret == DEVICE_OK);

	std::vector<std::string> opts;
	opts.push_back("None");
	opts.push_back("Shared");
	opts.push_back("Exclusive");

	ret = SetAllowedValues(g_PSULockProperty, opts);
	assert(ret == DEVICE_OK);
}
/*----------------------------------------------------------------------------*/
BK9130B::~BK9130B()
{
	if (initialized_)
	{
		Shutdown();
	}
}
/*----------------------------------------------------------------------------*/
int BK9130B::Initialize()
{
	if (initialized_)
	{
		return DEVICE_OK;
	}

	// set up active channel property
	CPropertyAction* pAct = new CPropertyAction(this, &BK9130B::OnActiveChannel);

	int ret = CreateProperty(g_PSUActiveChannelProperty, g_PSUActiveChannel_CH1, MM::String, false, pAct, false);
	assert(ret == DEVICE_OK);

	std::vector<std::string> opts;
	opts.push_back(g_PSUActiveChannel_CH1);
	opts.push_back(g_PSUActiveChannel_CH2);
	opts.push_back(g_PSUActiveChannel_CH3);

	ret = SetAllowedValues(g_PSUActiveChannelProperty, opts);
	assert(ret == DEVICE_OK);

	// set up output voltage property
	pAct = new CPropertyAction(this, &BK9130B::OnOutputVoltage);

	ret = CreateFloatProperty(g_PSUOutputVoltageProperty, 1.0, false, pAct, false);
	assert(ret == DEVICE_OK);

	ret = SetPropertyLimits(g_PSUOutputVoltageProperty, 0.0, 30.0);
	assert(ret == DEVICE_OK);

	// set up output current property
	pAct = new CPropertyAction (this, &BK9130B::OnOutputCurrent);

	ret = CreateFloatProperty(g_PSUOutputCurrentProperty, 0.0, false, pAct, false);
	assert(ret == DEVICE_OK);

	ret = SetPropertyLimits(g_PSUOutputCurrentProperty, 0.0, 3.0);
	assert(ret == DEVICE_OK);

	// get device id
	char idBuf[MM::MaxStrLength];

	ret = GetProperty(g_PSUDeviceIDProperty, idBuf);
	assert(ret == DEVICE_OK);

	idBuf[MM::MaxStrLength-1] = '\0';

	std::string devID(idBuf);

	// get device timeout
	ret = GetProperty(g_PSUDeviceIDProperty, timeout_);

	// get device lock mode
	char lockBuf[MM::MaxStrLength];
	ret = GetProperty(g_PSULockProperty, lockBuf);
	assert(ret == DEVICE_OK);

	lockBuf[MM::MaxStrLength-1] = '\0';

	std::string lockStr(lockBuf);

	ViAccessMode lockMode;

	if (lockStr == "None")
	{
		lockMode = VI_NO_LOCK;
	}
	else if (lockStr == "Shared")
	{
		lockMode = VI_SHARED_LOCK;
	}
	else if (lockStr == "Exclusive")
	{
		lockMode = VI_EXCLUSIVE_LOCK;
	}

	// open the device
	initialized_ = dev_.open(devID, lockMode, static_cast<ViUInt32>(timeout_));

	if (initialized_)
	{
		// register a clean up command that will be called on device close
		opts.clear();
		opts.push_back("INST:SEL CH1");
		opts.push_back("SOUR:CHAN:OUTP:STAT OFF");
		opts.push_back("INST:SEL CH2");
		opts.push_back("SOUR:CHAN:OUTP:STAT OFF");
		opts.push_back("INST:SEL CH3");
		opts.push_back("SOUR:CHAN:OUTP:STAT OFF");

		dev_.onClose(opts);

		// setup default values
		opts.clear();
		opts.push_back("INST:SEL CH1");
		opts.push_back("SOUR:CHAN:OUTP:STAT OFF");
		opts.push_back("SOUR:VOLT 1.0 V");
		opts.push_back("SOUR:CURR 0.0 A");
		dev_.write(opts);
	}
	else
	{
		LogMessage(dev_.getLastError());
	}

	return initialized_ ? DEVICE_OK : DEVICE_ERR;
}
/*----------------------------------------------------------------------------*/
int BK9130B::Shutdown()
{
	int ret = DEVICE_OK;

	if (initialized_)
	{
		if (!dev_.close())
		{
			LogMessage(dev_.getLastError());
			ret = DEVICE_ERR;
		}

		initialized_ = false;
	}

	return ret;
}
/*----------------------------------------------------------------------------*/
bool BK9130B::Busy()
{
	return false;
}
/*----------------------------------------------------------------------------*/
void BK9130B::GetName(char* name) const
{
   CDeviceUtils::CopyLimitedString(name, g_PSUName);
}
/*----------------------------------------------------------------------------*/
MM::DeviceType BK9130B::GetType() const
{
	return BK9310B_DEVICE_TYPE;
}
/*----------------------------------------------------------------------------*/
std::string BK9130B::doubleToStr(const double& val, const char& unit) const
{
	// 128 chars *SHOULD* be safe...
	char buf[128];
	sprintf(buf, "%f %c", val, unit);
	buf[127] = '\0';
	return std::string(buf);
}
/*----------------------------------------------------------------------------*/
int BK9130B::SetOpen(bool open)
{
	int ret = DEVICE_OK;

	if (open != activeChannelState_)
	{
		std::string stateStr = open ? "ON" : "OFF";

		// sending an channel select command (INST:SEL) souldn't be needed,
		// but we'll leave it for now just to be safe
		std::vector<std::string> cmd;
		cmd.push_back("INST:SEL " + activeChannel_);
		cmd.push_back("SOUR:CHAN:OUTP:STAT " + stateStr);

		if (dev_.write(cmd))
		{
			activeChannelState_ = open;
		}
		else
		{
			ret = ERR_WRITE_FAILED;
			LogMessage(dev_.getLastError());
		}
	}

	return ret;
}
/*----------------------------------------------------------------------------*/
int BK9130B::GetOpen(bool& state)
{
	int ret = DEVICE_OK;

	state = activeChannelState_;

	return ret;
}
/*----------------------------------------------------------------------------*/
int BK9130B::Fire(double duration)
{
	return DEVICE_UNSUPPORTED_COMMAND;
}
/*----------------------------------------------------------------------------*/
// sets the currently active channel
int BK9130B::OnActiveChannel(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	int ret = DEVICE_OK;

	if (eAct == MM::BeforeGet)
	{
		// user performed get operation
		std::string tmp = dev_.query("INST:SEL?");

		if (tmp.empty())
		{
			ret = ERR_QUERY_FAILED;
			LogMessage(dev_.getLastError());
		}
		else
		{
			pProp->Set(tmp.c_str());
			activeChannel_ = tmp;
		}
	}
	else if (eAct == MM::AfterSet)
	{
		// user performed set operation
		pProp->Get(activeChannel_);
		if (!dev_.write("INST:SEL " + activeChannel_))
		{
			ret = ERR_WRITE_FAILED;
			LogMessage(dev_.getLastError());
		}
		else
		{
			// make sure our activeChannelState_ is up-to-date
			// NOTE: we probably should check that the query was successful...
			activeChannelState_ = dev_.query("SOUR:CHAN:OUTP:STAT?") == "1";
		}
	}

	return ret;
}
/*----------------------------------------------------------------------------*/
int BK9130B::OnOutputVoltage(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	return OnOutputChange(pProp, eAct, outputVoltage_, 'V');
}
/*----------------------------------------------------------------------------*/
int BK9130B::OnOutputCurrent(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	return  OnOutputChange(pProp, eAct, outputCurrent_, 'A');
}
/*----------------------------------------------------------------------------*/
int BK9130B::OnOutputChange(MM::PropertyBase* pProp, MM::ActionType eAct, double& value, const char& unit)
{
	int ret = DEVICE_OK;

	std::string cmd = unit == 'A' ? "SOUR:CURR" : "SOUR:VOLT";

	if (eAct == MM::BeforeGet)
	{
		// user triggered get request
		std::string tmp = dev_.query(cmd + ":LEV?");

		if (tmp.empty())
		{
			ret = ERR_QUERY_FAILED;
			LogMessage(dev_.getLastError());
		}
		else
		{
			value = strtod(tmp.c_str(), NULL);
			pProp->Set(value);
		}
	}
	else if (eAct == MM::AfterSet)
	{
		// user triggered set request
		pProp->Get(value);

		// unlike CH1 and 2, CH3 has a 5V limit...
		if ((activeChannel_ == "CH3") && (unit == 'V') && (value > 5.0))
		{
			value = 5.0;
			ret = ERR_INVALID_VOLTAGE;
		}

		std::string valueStr = doubleToStr(value, unit);

		if (!dev_.write(cmd + " " + valueStr))
		{
			ret = ERR_WRITE_FAILED;
			LogMessage(dev_.getLastError());
		}
	}

	return ret;
}
/*============================================================================*/
