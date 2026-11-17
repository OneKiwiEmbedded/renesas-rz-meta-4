# OneKiwi RZ/V2H boards: TF-A 2.10 (Renesas 3c83dd6f49).
# Board setting is selected with TFA_BOARD in the machine conf (4GB: onekiwi_8gb, 8GB: evk_1).
TFA_URI:onekiwi-rzv2h-4gb = "git://github.com/OneKiwiEmbedded/renesas-rz-atf-4.git;protocol=https"
TFA_URI:onekiwi-rzv2h-8gb = "git://github.com/OneKiwiEmbedded/renesas-rz-atf-4.git;protocol=https"

# branch 2.10.5/rzv2h_1.1.0
# 4GB: 9c0f8dd "onekiwi_8gb: convert LPDDR4 setting to TF-A 2.10 format" (needed by TFA_BOARD=onekiwi_8gb)
TFA_REV:onekiwi-rzv2h-4gb = "9c0f8dd83fde3837e5268f232c50f00ac4d1a28e"
# 8GB: 8160111 "add board v2h_onekiwi_8gb" (8GB uses TFA_BOARD=evk_1)
TFA_REV:onekiwi-rzv2h-8gb = "8160111194f7f7dd92bfe499c9127537386ce56f"
