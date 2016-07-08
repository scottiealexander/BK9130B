## BK9130B
A Micro-Manager device adapter for BK Precision 9130B programmable power supply

See the [Micro-Manager](https://micro-manager.org/) website for further information about Micro-Manager.

### Installing

#### From distributed dll
* The distributed dll within the **/bin** directory is compatible with 64bit Micro-Manager version 1.4.23_20160707 [(Download Link)](http://valelab.ucsf.edu/~MM/nightlyBuilds/1.4/Windows/MMSetup_64bit_1.4.23_20160707.exe) and in theory can just be dropped into the Micro-Manager install directory.

#### From source
* Follow the [Micro-Manager build instructions](https://micro-manager.org/wiki/Building_and_debugging_Micro-Manager_source_code) for acquiring source code and setting up a build environment.
    1. The contents of this repo can be copied into the TestDeviceAdapters sub-directory and the the project can be added to the larger **micromanager.sln** solution file (for MSVC 2010).
    2. Download NI-VISA drivers for your platform from [HERE](http://www.ni.com/nisearch/app/main/p/bot/no/ap/tech/lang/en/pg/1/sn/catnav:du,n8:3.25.123.1640,ssnav:ndr/).
    3. Edit the include and link directories list for the BK9130B MSVC project to include the paths to the **Include** and **Lib_x64** directories (respectively) that should be located within the IVI Foundation directory within the downloaded / installed package (the required header files are **visa.h** and **visatyp.h**, and the required library files are (on Windows) are **visa32**/**visa64.lib**).
    4. When in doubt refer to the Micro-Manager website (e.g. [HERE](https://micro-manager.org/wiki/Building_MM_on_Windows)).

### Testing
* Source code (**test_console.cpp**) and x64 Windows exe (**/bin/test_console.exe**) are included for testing the VISADevice class from a console-like interface. The test code does not require Micro-Manager, but does require VISADevice.h, the NI-VISA library / header files, and a c++11 capable compiler. See **bin/contents.md** for more information.

### Notes
* This device adapter has only been tested with the USB interface, other interfaces (RS232, GPIB) may or may not work.

## License
* This work is licensed under the BSD 2.0 license. See **LICENSE** file for more information.
