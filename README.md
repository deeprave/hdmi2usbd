HDMI2USBd
---------

hdmi2usbd is a daemon designed for communicating with HDMI2USB devices, but may 
work for other purposes.

This software implements a daemon that provides a tcp gateway to a local serial
port primarily to provide (more) reliable communication with an hdmi2usb device
to compensate for lack of flow control and internal buffering. In this mode it 
may also be useful for other purposes.

Additional objectives of this software planned to be implemented:

 - python 3.4+ bindings, including asyncio interface, switching devices,
   daemon control & device state introspection
 - additional functionality to provide event callbacks by selectable events
   at a high level
 

*Status*: this is work in progress but becoming close to basically functional.

