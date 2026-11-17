# 4GB board: the Renesas EVK LPDDR4 setting (TFA_BOARD=evk_1) fails DDR training
# on the OneKiwi 4GB SOM ("BL2: ERROR: Training failed", 2026-11-18).
# Use the OneKiwi LPDDR4 setting (the one the SDK v5.20 TF-A 2.7 ran on 4GB and 8GB),
# converted to the TF-A 2.10 format: OneKiwiEmbedded/renesas-rz-atf-4, board v2h_onekiwi_8gb.
TFA_URI:onekiwi-rzv2h-4gb = "git://github.com/OneKiwiEmbedded/renesas-rz-atf-4.git;protocol=https"
TFA_REV:onekiwi-rzv2h-4gb = "9c0f8dd83fde3837e5268f232c50f00ac4d1a28e"
