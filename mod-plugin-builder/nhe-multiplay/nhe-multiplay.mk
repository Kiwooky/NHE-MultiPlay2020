######################################
#
# nhe-multiplay
#
# MultiPlay 20/20 by New Horizon Electronics
# https://github.com/Kiwooky/NHE-MultiPlay2020
#
# This file is the plugin's package for mod-plugin-builder
# (plugins/package/nhe-multiplay/nhe-multiplay.mk). The same file can be
# uploaded to https://builder.mod.audio/buildroot to get an install link.
#
# Set NHE_MULTIPLAY_VERSION to the full hash of the commit to build.
#
######################################

NHE_MULTIPLAY_VERSION = COMMIT_HASH_HERE
NHE_MULTIPLAY_SITE = https://github.com/Kiwooky/NHE-MultiPlay2020.git
NHE_MULTIPLAY_SITE_METHOD = git
NHE_MULTIPLAY_GIT_SUBMODULES = y
# fetch git submodules (DPF), as MOD's own packages do (mod-plugin-builder)
NHE_MULTIPLAY_PRE_DOWNLOAD_HOOKS += MOD_PLUGIN_BUILDER_DOWNLOAD_WITH_SUBMODULES

NHE_MULTIPLAY_BUNDLES = nhe-multiplay.lv2

NHE_MULTIPLAY_TARGET_MAKE = $(TARGET_MAKE_ENV) $(TARGET_CONFIGURE_OPTS) $(MAKE) NOOPT=true -C $(@D)

define NHE_MULTIPLAY_BUILD_CMDS
	$(NHE_MULTIPLAY_TARGET_MAKE)
endef

define NHE_MULTIPLAY_INSTALL_TARGET_CMDS
	$(NHE_MULTIPLAY_TARGET_MAKE) install DESTDIR=$(TARGET_DIR) PREFIX=/usr
endef

$(eval $(generic-package))
