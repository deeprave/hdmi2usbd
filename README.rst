hdmi2usbd
=========

**hdmi2usbd** is a daemon (but runs in foreground by default) designed
for reliably communicating with HDMI2USB devices, but may work for other
purposes such as providing a simple serial to network gateway that supports
both ipv4 and ipv6. It was written to compensate for lack of flow control
and internal buffering in the hdmi2usb Opsis and Atlys board firmware, and
allows concurrent connection from multiple clients, consumers and controllers.

Additional objectives of this software being implemented:

`hdmi2usbmon
<https://github.com/deeprave/hdmi2usbmon>`_

- hdmi2usbmon is a python package interface to hdmi2usb devices, employing
  hdmi2usbd to connect to a device. This package will provide an API
  suitable for querying and controlling HDMI2USB devices.

  It will initially provide a sample client in the form of a status and
  event logger, paving the way for a status and control application
  and web interface.

--------------

