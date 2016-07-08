////////////////////////////////////////////////////////////////////////////////
// FILE:          VISADevice.h
// PROJECT:       Micro-Manager
// SUBSYSTEM:     DeviceAdapters
//-----------------------------------------------------------------------------
// DESCRIPTION:   Base class for VISA/SCPI devices
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

/*GIST
  VISADevice will not inherit from any MM devices so that subclasses can,
  we'll just have to be careful with our method names (or force composition)
*/
#pragma once
#ifndef _VISADEVICE_H_
#define _VISADEVICE_H_

#include <sstream>
#include <vector>
#include <string>

/*use boost if c++11 is not supported (NOTE: compilers are known to lie so
  if c++11 is not actually supported issues may arise, otherwise boost fallback
  should work)
*/
#if defined(__MSC_VER) || !(__cplusplus > 199711L)
    // building with Micro-Manager / require boost
    #define BK9130B_USE_BOOST
    #include <boost/type_traits/is_arithmetic.hpp>
    #include <boost/static_assert.hpp>
    #include <boost/thread.hpp>
    #include <boost/chrono.hpp>
#else
    // with c++11 we don't need boost...
    #include <type_traits>
    #include <thread>
    #include <chrono>
#endif

#include "visa.h"

// NOTE: according to the NI-VISA documentation, this must be *at least* 256
#define ERROR_MSG_MAX 512 //maximum length of error description

// WARNING: this is only a guess, there is very little documentation that I've
// been able to find to suggest the propter way to deal with string attributes
// but the examples that I've seen only use 256 (i.e. VI_FIND_BUFLEN)
#define ATTR_MAX_LENGTH 1024 //maximum length of string attributes

/*TODO: get copies of libvisa for Darwin and Linux for our lib subfolder*/

/*============================================================================*/
/**
* Concatonates the elements of a container into a string inserting the string
* <sep> in between each.
* NOTE: seperator is placed between elements and *NOT* at the end of the
* resultant string
* @param first - iterator for the begining of the join operation
* @param last - iterator indicating the end of the join operation
* @param sep - a string to place between elements of <list> (see not above)
* @return - the resultant string
*/
template <typename Iterator>
std::string join(Iterator first, Iterator last, const std::string& sep)
{
    std::ostringstream result;
    bool addSep = false;

    for (Iterator it = first; it != last; ++it)
    {
        if (addSep)
        {
            result << sep;
        }

        result << *it;

        addSep = true;
    }

    return result.str();
}
/*============================================================================*/
class VISADevice
{
public:
    /*------------------------------------------------------------------------*/
    VISADevice() : closeCmd_(""), lastError_("")
    {
        // NOTE: creating and destroying a session does not require
        // communication with a device (and is cheap), and we need to initialize
        // the session to be able to find instruments
        ViStatus status = viOpenDefaultRM(&session_);
        if (processStatus(status))
        {
            initialized_ = true;
        }
    }
    /*------------------------------------------------------------------------*/
    ~VISADevice()
    {
        // close the session if it was successfully initialized
        // this doesn't invlove communication with the device unless the
        // device is open (i.e. the user forgot to call close())
        if (initialized_)
        {
            if (open_)
            {
                close();
            }

            if (!open_)
            {
                // if close is sucessful, set session init state to false
                if (processStatus(viClose(session_)))
                {
                    initialized_ = false;
                }
            }
        }
    }
    /*------------------------------------------------------------------------*/
    bool open(std::string deviceStr,
        ViAccessMode accessMode = VI_NO_LOCK,
        ViUInt32 timeout = 2000)
    {
        bool success = false;

        timeout_ = timeout;

        if (initialized_)
        {
            char* device_nc = const_cast<char*>(deviceStr.c_str());
            ViStatus status = viOpen(session_, device_nc, accessMode, timeout,
                &device_);

            // if open was successful, mark device as open
            if (processStatus(status))
            {
                open_ = true;

                // get the termination character for writes
                success = processStatus(viGetAttribute(device_,
                    VI_ATTR_TERMCHAR, &termChar_));

                // if we failed to get the termChar_, just close down as we
                // can't safetly perform any write operations
                if (!success)
                {
                    close();
                }
            }
        }

        return success;
    }
    /*------------------------------------------------------------------------*/
    bool close()
    {
        if (open_)
        {
            if (!closeCmd_.empty())
            {
                if (!write(closeCmd_))
                {
                    lastError_ = "[WARN]: failed to send onClose command!\n";
                }
            }

            if (processStatus(viClose(device_)))
            {
                open_ = false;
            }
        }

        return !open_;
    }
    /*------------------------------------------------------------------------*/
    bool isInitialized() const
    {
        return initialized_;
    }
    /*------------------------------------------------------------------------*/
    bool isOpen() const
    {
        return open_;
    }
    /*------------------------------------------------------------------------*/
    void onClose(const std::string& cmd)
    {
        closeCmd_ = cmd;
    }
    /*------------------------------------------------------------------------*/
    void onClose(const std::vector<std::string>& cmds)
    {
        closeCmd_ = join(cmds.begin(), cmds.end(), getCmdSeperator());
    }
    /*------------------------------------------------------------------------*/
    std::vector<std::string> findInstruments(const std::string& expr)
    {
        std::vector<std::string> instrList;

        // device communication not required, only check for valid session
        if (initialized_)
        {
            ViFindList findList;
            ViUInt32 retSize;

            ViChar *buf = new ViChar[VI_FIND_BUFLEN];

            char* expr_nc = const_cast<char*>(expr.c_str());

            if (processStatus(viFindRsrc(session_, expr_nc, &findList,
                &retSize, buf)))
            {

                instrList.resize(retSize);

                instrList[0] = std::string(buf, VI_FIND_BUFLEN);

                for (std::vector<std::string>::size_type k = 1; k < retSize;
                    ++k)
                {
                    if (processStatus(viFindNext(findList, buf)))
                    {
                        // allow ctor to perform truncation, don't speficy size
                        buf[VI_FIND_BUFLEN-1] = '\0';
                        instrList[k] = std::string(buf);
                    }
                    else
                    {
                        break;
                    }
                }
            }

            delete[] buf;
        }

        return instrList;
    }
    /*------------------------------------------------------------------------*/
    bool setAttribute(ViAttr attribute, ViAttrState state)
    {
        // NOTE: ViAttrState is either a ViUInt32 or ViUInt64 depending on the
        // system (i.e. only integer attributes can be set)
        bool success = false;

        if (open_)
        {
            success = processStatus(viSetAttribute(device_, attribute, state));
        }

        return success;
    }
    /*------------------------------------------------------------------------*/
    template <typename T>
    bool getScalarAttribute(ViAttr attribute, T* ptr)
    {
#ifdef BK9130B_USE_BOOST
        BOOST_STATIC_ASSERT_MSG(boost::is_arithmetic<T>::value,
            "Input/return type must be arithmetic");
#else
        static_assert(std::is_arithmetic<T>::value,
            "Input/return type must be arithmetic");
#endif
        bool success = false;

        if (open_)
        {
            success = processStatus(viGetAttribute(device_, attribute, ptr));
        }

        return success;
    }
    /*------------------------------------------------------------------------*/
    std::string getStringAttribute(ViAttr attribute)
    {
        std::string attr("");

        if (open_)
        {
            char* buf = new char[ATTR_MAX_LENGTH];

            processStatus(viGetAttribute(device_, attribute, buf));

            // make sure that the last char is null, then let the string
            // constructor truncate the string to the first null as needed
            buf[ATTR_MAX_LENGTH-1] = '\0';

            attr = std::string(buf);

            delete[] buf;
        }

        return attr;
    }
    /*------------------------------------------------------------------------*/
    bool write(const std::string& msg)
    {
        // NOTE: we make room for only the characters we need (i.e. the chars
        // in the msg string +1 for the termChar_, no null termination)
        ViUInt32 bufSize = static_cast<ViUInt32>(msg.length() + 1);

        ViByte *buf = new ViByte[bufSize];

        std::copy(msg.begin(), msg.end(), buf);

        // add the terminating character
        buf[bufSize-1] = static_cast<ViByte>(termChar_);

        bool success = write(buf, bufSize);

        delete[] buf;

        return success;
    }
    /*------------------------------------------------------------------------*/
    bool write(const std::vector<std::string>& list)
    {
        return write(join(list.begin(), list.end(), getCmdSeperator()));
    }
    /*------------------------------------------------------------------------*/
    // NOTE: we are not overloading query with a vector of strings form as it
    // appears that the device only response to the last query if multiple
    // query commands are sent in a single write
    std::string query(const std::string& msg)
    {
        std::string reply("");

        bool success = write(msg);

        if (success)
        {
#ifdef BK9130B_USE_BOOST
            boost::this_thread::sleep_for(
                boost::chrono::milliseconds(timeout_));
#else
            std::this_thread::sleep_for(std::chrono::milliseconds(timeout_));
#endif
            reply = read();
        }

        return reply;
    }
    /*------------------------------------------------------------------------*/
    std::string read(const ViUInt32 bufSize = 0x00000400)
    {
        std::string reply("");

        if (initialized_ && open_)
        {
            ViByte *buf = new ViByte[bufSize];

            ViUInt32 retSize;

            if (processStatus(viRead(device_, buf, bufSize, &retSize)))
            {
                reply = std::string(reinterpret_cast<char*>(buf), retSize);
            }

            delete[] buf;
        }

        return reply;
    }
    /*------------------------------------------------------------------------*/
    std::string getDeviceDescription()
    {
        std::string desc("");

        if (open_)
        {
            desc += (getStringAttribute(VI_ATTR_MANF_NAME) + " : "
                + getStringAttribute(VI_ATTR_MODEL_NAME) + " : "
                + getStringAttribute(VI_ATTR_INTF_INST_NAME));
        }

        return desc;
    }
    /*------------------------------------------------------------------------*/
	std::string getLastError()
	{
		std::string tmp = lastError_;
		lastError_ = "";
		return tmp;
	}
	/*------------------------------------------------------------------------*/

private:
    /*------------------------------------------------------------------------*/
    bool processStatus(ViStatus status)
    {
        bool success = false;

        if (status < VI_SUCCESS)
        {
			ViSession tmp;

            if (open_ || initialized_)
			{
				if (open_)
				{
					// NOTE: we are assuming that the error pertains to the device
					tmp = device_;
				}
				else
				{
					// the error likely pertains to the session
					tmp = session_;
				}

				char buf[ERROR_MSG_MAX];

				viStatusDesc(tmp, status, buf);

				buf[ERROR_MSG_MAX-1] = '\0';

				lastError_ = std::string(buf);
			}
            else
            {
                lastError_ = "Neither session nor device is initialized";
            }

        }
		else
		{
			success = true;
		}

        return success;
    }
    /*------------------------------------------------------------------------*/
    bool write(ViByte* msg, ViUInt32 msgSize)
    {
        bool success = false;

        if (initialized_ && open_)
        {
            // TODO: not sure if we should check nWritten agains msgSize, or if
            // the return status handles all issues that may arise...
            ViUInt32 nWritten;

            success = processStatus(viWrite(device_, msg, msgSize,
                &nWritten));
        }

        return success;
    }
    /*------------------------------------------------------------------------*/
    std::string getCmdSeperator() const
    {
        std::string sep(";");
        sep.append(1, static_cast<char>(termChar_));

        return sep;
    }
    /*------------------------------------------------------------------------*/

private:
    ViSession session_;
    ViSession device_;
    bool initialized_;
    bool open_;

	std::string closeCmd_;

	std::string lastError_;

private:
    ViUInt8 termChar_;
    ViUInt32 timeout_;
};
/*============================================================================*/
#endif //_VISADEVICE_H_
