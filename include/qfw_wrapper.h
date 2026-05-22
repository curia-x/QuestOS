/* SPDX-License-Identifier: GPL-2.0 */
#ifndef QFW_WRAPPER_H
#define QFW_WRAPPER_H

int qfw_load_images(void);
char *qfw_get_bootargs(void);
char *qfw_get_kernel_image_addr(void);
int qfw_init(void);

#endif /* QFW_WRAPPER_H */