///////////////////////////////////////////////////////////////////////////////
// FILE:          BK9130B.h
// PROJECT:       Micro-Manager
// SUBSYSTEM:     DeviceAdapters
//-----------------------------------------------------------------------------
// DESCRIPTION:   Device adapter for BK9130B using the NI-VISA drivers
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

#pragma once
#ifndef _BK9130B_H_
#define _BK9130B_H_

#include "DeviceBase.h"
#include "VISADevice.h"

/*------------------------------------------------------------------------------
  Error codes
------------------------------------------------------------------------------*/
#define ERR_INVALID_CHANNEL      102
#define ERR_INVALID_VOLTAGE      103
#define ERR_INVALID_CURRENT      104
#define ERR_WRITE_FAILED		 105
#define ERR_READ_FAILED 		 106
#define ERR_QUERY_FAILED 		 107

/*----------------------------------------------------------------------------*/
// device type as used by GetType() and InitializeModuleData()
#define BK9310B_DEVICE_TYPE MM::ShutterDevice

/*============================================================================*/

class BK9130B : public CShutterBase<BK9130B>
{
public:
	BK9130B(void);
	~BK9130B(void);

	// MMDevice API
    // ------------
    int Initialize(void);
    int Shutdown(void);

	bool Busy(void);

    void GetName(char* name) const;
	MM::DeviceType GetType(void) const;

	// Shutter API
	// -----------
    int SetOpen(bool open = true);
    int GetOpen(bool&);
    int Fire(double);

	// Action Interface
	// ----------------
	int OnActiveChannel(MM::PropertyBase*, MM::ActionType);
	int OnOutputVoltage(MM::PropertyBase*, MM::ActionType);
	int OnOutputCurrent(MM::PropertyBase*, MM::ActionType);

private:
	int OnOutputChange(MM::PropertyBase*, MM::ActionType, double&, const char&);
	std::string doubleToStr(const double&, const char&) const;

private:
    VISADevice dev_;
	bool initialized_;
	bool busy_;
	long timeout_;

private:
	std::string activeChannel_;
	bool activeChannelState_;
	double outputVoltage_;
	double outputCurrent_;
};
/*============================================================================*/
#endif //_BK9130B_H_
