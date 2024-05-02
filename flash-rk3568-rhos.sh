#!/bin/bash

echo Bootloader
./../rkbin/tools/rkdeveloptool db ./../rkbin/rk356x_spl_loader_v1.21.113.bin
echo UEFI SD-card image
sleep 1
./../rkbin/tools/rkdeveloptool wl 0 RK3568-RHOS_EFI.img
echo Reset board
sleep 1
./../rkbin/tools/rkdeveloptool rd
