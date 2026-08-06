"""Minimal bulk read/write to a USB printer-class device."""

import contextlib
import time

import usb.core
import usb.util


def open_dev(vid, pid):
    dev = usb.core.find(idVendor=vid, idProduct=pid)
    if dev is None:
        raise SystemExit(f"not found {vid:04x}:{pid:04x}")
    cfg = dev.get_active_configuration()
    intf = cfg[(0, 0)]
    try:  # macOS has nothing to detach and refuses the call
        if dev.is_kernel_driver_active(0):
            dev.detach_kernel_driver(0)
    except (usb.core.USBError, NotImplementedError):
        pass
    usb.util.claim_interface(dev, 0)
    # The Brother needs alt setting 0 forced after a claim; others refuse the call.
    with contextlib.suppress(usb.core.USBError):
        dev.set_interface_altsetting(interface=0, alternate_setting=0)
    ep_out = usb.util.find_descriptor(intf, custom_match=lambda e: usb.util.endpoint_direction(e.bEndpointAddress) == usb.util.ENDPOINT_OUT)
    ep_in = usb.util.find_descriptor(intf, custom_match=lambda e: usb.util.endpoint_direction(e.bEndpointAddress) == usb.util.ENDPOINT_IN)
    return dev, ep_out, ep_in


def write(ep_out, data, chunk=64):
    for i in range(0, len(data), chunk):
        ep_out.write(data[i : i + chunk], timeout=5000)


def read_exact(ep_in, n, timeout=3000, settle=0.2):
    time.sleep(settle)
    buf = bytearray()
    while len(buf) < n:
        try:
            buf += bytes(ep_in.read(min(64, n - len(buf)), timeout=timeout))
        except usb.core.USBTimeoutError:
            break
        except usb.core.USBError as e:
            if e.errno in (60, 110):
                break
            raise
    return bytes(buf)
