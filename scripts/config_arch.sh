pacman -S screen cargo rustup cmake ninja clang gcc llvm boost bearssl python uv docker-buildx
rustup default stable

mkdir /tmp/setuprust
cd /tmp/setuprust
git clone git@github.com:obliviouslabs/rostl.git
cd rostl
sh scripts/setup_dev_end.sh