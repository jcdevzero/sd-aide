# sd-aide

This repository contains the source code for the system extension portion of **SD Aide**. What is SD Aide?

### Problem

Some users (me and a few others) have experienced lock ups in our PowerBooks when SCSI emulation devices (ZuluSCSI, BlueSCSI, etc) are spun down for power conservation and then subsequently spun back up. SCSI2SD seems to not have this issue but it seems some folks using the other two major brands (ZuluSCSI and BlueSCSI) have had issues. For those of us having issues, it has occurred in System 7.1 through System 7.5.5.

### Why does it happen? (Working Theory)

It seems after power is restored to these SCSI emulators, the system software will quickly (within 500ms) issue the IO request that caused the SCSI emulator to be spun back up (ie. have its power restored). If the SCSI emulator device isn't ready when this IO request comes in, you get into this hung state because the SCSI emulator misses the request and sits around waiting for a request, whereas the system software issued the request and sits around waiting for a response. This lockup lasts for 180 seconds after which the system finally unfreezes.

### Why are some devices ok?

I believe it's all in the timing. If the device can become operational before the IO request comes in, you're all good. Some devices (and/or their firmware version) either get ready quickly or get into a state where it can read and hold the IO request until the device is fully ready to go (ie. don't miss the request). I have not been able to reproduce the issue with a SCSI2SD device. It's possible that's because SCSI2SD uses the SD card as a raw disk whereas ZuluSCSI and BlueSCSI have an extra layer (filesystem) from which it exposes multiple disks. Or it could be those other two devices offer more features and it takes more time to get ready? I have heard newer BlueSCSI's might be ok? If you aren't having these issues, awesome! But if you are, continue reading.

### Solution

SD Aide is a system extension / control panel which intercepts the HD spin up function in the system software and after spin up, adds a configurable delay to allow the device to finish booting before the IO request is sent. This ensures the SCSI emulator device does not miss the request so that your PowerBook isn't locked up for 3 minutes every time the disk is spun down. You can of course just disable spin down but this option doesn't exist in System 7.1 through 7.1.1, and that wouldn't help with sleep mode.

### More Info Here

https://68kmla.org/bb/index.php?threads/sd-aide-for-scsi-emulator-power-up-lock-up-issues.47822/
