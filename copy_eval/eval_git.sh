cd ~/SSD2/eval
rm -r .git
git init
cd ~/sgx_ssd_kernel/copy_eval
sh drop_cache.sh
time ./git.sh
