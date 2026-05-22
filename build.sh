#!/bin/sh
# SPDX-License-Identifier: GPL-2.0

PROJECT_BUILD_DIR=build

build() {
	make OUT_DIR=${PROJECT_BUILD_DIR} -j$(nproc)
}

build_app() {
	make OUT_DIR=${PROJECT_BUILD_DIR} -j$(nproc) app
}

build_kernel() {
	make OUT_DIR=${PROJECT_BUILD_DIR} -j$(nproc) kernel
}

clean() {
	make OUT_DIR=${PROJECT_BUILD_DIR} -j$(nproc) clean
}

run() {
	make  OUT_DIR=${PROJECT_BUILD_DIR} -j$(nproc) && \
    sh ./qemu_run.sh -bios ${PROJECT_BUILD_DIR}/quest_os_pack_all.bin
}

run_kernel() {
	make  OUT_DIR=${PROJECT_BUILD_DIR} -j$(nproc) && \
    sh ./qemu_run.sh -bios ${PROJECT_BUILD_DIR}/quest_os_kernel.bin
}

debug() {
	make OUT_DIR=${PROJECT_BUILD_DIR} -j$(nproc) && \
    sh ./qemu_run.sh -bios ${PROJECT_BUILD_DIR}/quest_os_pack_all.bin debug
}

qemu_info() {
	sh ./qemu_run.sh info
}

# exec_arg() {
# 	case "$1" in
# 	clean)
# 		make OUT_DIR=${PROJECT_BUILD_DIR} -j$(nproc) clean
# 		;;
# 	build)
# 		make OUT_DIR=${PROJECT_BUILD_DIR} -j$(nproc)
# 		;;
#     qemu_info)
#         sh ./qemu_run.sh info
#         ;;
#     debug)
# 		make OUT_DIR=${PROJECT_BUILD_DIR} -j$(nproc)
#         sh ./qemu_run.sh -bios ${PROJECT_BUILD_DIR}/quest_os.bin debug
#         ;;
#     run)
# 		make -j$(nproc)
#         sh ./qemu_run.sh -bios ${PROJECT_BUILD_DIR}/quest_os.bin
#         ;;
#     *)
#         echo "用法: $0 {build|clean|run|debug|qemu_info|}"
#         exit 1
#         ;;
# 	esac
# }

# if [ $# -lt 1 ]; then
# 	exec_arg run
# fi

# for arg in "$@"; do
#     exec_arg $arg
# done
