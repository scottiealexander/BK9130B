/*------------------------------------------------------------------------------
  Description: A simple, console based test program for VISADevice class

  Build:
    <VISA_INCLUDE> = path to NI-VISA include directory
    <VISA_LIB> = path to NI-VISA library directory
    g++ -std=c++11 -I. -I${VISA_INCLUDE} -L${VISA_LIB} -o \
    test_console test_console.cpp -lvisa64

  Updated: 2016-07-08

  Author: Scottie Alexander, scottiealexander11@gmail.com

  Copyright: University of California, Davis, 2016

  License: This file is distributed under the BSD license.
           License text is included with the source distribution.

           This file is distributed in the hope that it will be useful,
           but WITHOUT ANY WARRANTY; without even the implied warranty
           of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

           IN NO EVENT SHALL THE COPYRIGHT OWNER OR
           CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
           INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES.
------------------------------------------------------------------------------*/
#include <iostream>
#include <istream>
#include <vector>
#include <string>

// requires c++11 (or linking with boost)
#include <regex>

#include "VISADevice.h"

/*----------------------------------------------------------------------------*/
void logMessage(const std::string& msg, const std::string& prefix = "[REC]: ",
    std::ostream& io = std::cout)
{
    io << prefix << msg << std::endl;
}
/*----------------------------------------------------------------------------*/
void usage()
{
    const std::string  msg =
    "\n------------------------------------------------------\n"
    "Command set:\n\t"
    "r - read from device\n\t"
    "w <msg> - write <msg> to device\n\t"
    "q <msg> - write <msg> to device and read reply\n\t"
    "h - print this help message\n\t"
    "exit - exit console\n"
    "------------------------------------------------------\n";

    logMessage(msg, "");
}
/*----------------------------------------------------------------------------*/
char parseMessage(const std::string& imsg, std::string& omsg)
{
    char cmd;
    std::smatch sm;
    std::regex ex("\\s*(\\w+)[^\\w\\*]*(.*)");

    if (std::regex_search(imsg, sm, ex) && sm.size() > 1)
    {
        omsg = sm.str(2);
        cmd = sm.str(1)[0];
    }
    else
    {
        logMessage("regex match fail - " + imsg, "[WARN]: ");
        omsg = "";
        cmd = '?';
    }

    return cmd;
}
/*----------------------------------------------------------------------------*/
int main()
{

    VISADevice dev;

    // only look for USB devices
    std::vector<std::string> inst = dev.findInstruments("USB?*");

    if (inst.size() < 1)
    {
        logMessage("Failed to find device!", "[ERROR]: ", std::cerr);
        return -1;
    }

    if (!dev.open(inst[0]))
    {
        logMessage("Failed to open device!", "[ERROR]: ", std::cerr);
        return -2;
    }

    logMessage("Connected to device - " + dev.getDeviceDescription(),
        "[IFO]: ");

    dev.onClose({
        "INST:SEL CH1",
        "SOUR:CHAN:OUTP:STAT OFF",
        "INST:SEL CH2",
        "SOUR:CHAN:OUTP:STAT OFF",
        "INST:SEL CH3",
        "SOUR:CHAN:OUTP:STAT OFF"
    });

    // print console usage
    usage();

    std::string msg;

    while (std::cin)
    {
		std::cout << ">>> ";

        std::getline(std::cin, msg);

		if (msg == "exit")
		{
			break;
		}
		else
		{
            std::string omsg;
            char cmd = parseMessage(msg, omsg);

			switch (cmd)
            {
                case 'r':
                case 'R':
                    logMessage("...", "[READ]");
                    logMessage(dev.read());
                    break;
                case 'w':
                case 'W':
                    logMessage(omsg, "[WRITE]: ");
                    if (!dev.write(omsg))
                    {
                        logMessage(dev.getLastError(), "[ERROR]: ", std::cerr);
                    }
                    break;
                case 'q':
                case 'Q':
                    logMessage(omsg, "[QUERY]: ");
                    logMessage(dev.query(omsg));
                    break;
                case 'h':
                case 'H':
                    usage();
                    break;
                case '?':
                    logMessage("Invalid command!", "[ERROR]: ", std::cerr );
                    usage();
                    break;
                default:
                    logMessage("Command does not match.", "[ERROR] :",
                        std::cerr);
                    usage();
                    break;
            }
		}
    }

    return 0;
}
/*----------------------------------------------------------------------------*/
