// SPDX-License-Identifier: GPL-2.0
.section ".text.entry","ax"

.global _proc_start
_proc_start:
	# jump to nolibc _start
	b	_start
