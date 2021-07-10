## Step 1: Install dependencies, see https://wiki.qemu.org/Hosts/Linux
PACKAGES=git \
         libglib2.0-dev \
         libfdt-dev \
         libpixman-1-dev \
         zlib1g-dev \
         libaio-dev \
         libbluetooth-dev \
         libbrlapi-dev \
         libbz2-dev \
         libcap-dev \
         libcap-ng-dev \
         libcurl4-gnutls-dev \
         libgtk-3-dev \
         libibverbs-dev \
         libjpeg8-dev \
         libncurses5-dev \
         libnuma-dev \
         librbd-dev \
         librdmacm-dev \
         libsasl2-dev \
         libsdl1.2-dev \
         libseccomp-dev \
         libsnappy-dev \
         libssh2-1-dev \
         libvde-dev \
         libvdeplug-dev \
         libvte-2.90-dev \
         libxen-dev \
         liblzo2-dev \
         libnfs-dev \
         libiscsi-dev


sudo apt-get update && \
    DEBIAN_FRONTEND=noninteractive sudo apt-get -y install --no-install-recommends $PACKAGES
# sudo dpkg -l $PACKAGES | sort > /packages.txt

# Apply patch https://reviews.llvm.org/D75820
# This is required for TSan in clang-10 to compile with QEMU.
sudo sed -i 's/^const/static const/g' /usr/lib/llvm-10/lib/clang/10.0.0/include/sanitizer/tsan_interface.h