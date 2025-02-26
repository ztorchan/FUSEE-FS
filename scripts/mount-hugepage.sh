set -euxo pipefail

mkdir -p /dev/hugepages
mountpoint -q /dev/hugepages || mount -t hugetlbfs nodev /dev/hugepages
echo 10240 > /sys/devices/system/node/node0/hugepages/hugepages-2048kB/nr_hugepages