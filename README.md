# hdmi2usbd

---
This software is part of the [HDMI2USB project](https://github.com/timvideos/HDMI2USB).

---

**hdmi2usbd** is a daemon (but runs in foreground by default) designed for reliably communicating with HDMI2USB devices, but may work for other purposes such as providing a simple serial to network gateway. It compensates for lack of flow control and internal buffering in the hdmi2usb Opsis and Atlys boards.

Additional objectives of this software planned to be implemented:

 - python 3.4+ bindings, including asyncio interface, switching devices, daemon control & device state introspection
 - additional functionality to provide event callbacks by selectable events at a high level


---
_**Status**: this is work in progress but becoming close to basically functional._

