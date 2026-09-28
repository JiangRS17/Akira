/* SPDX-License-Identifier: BSD-3-Clause */
#ifndef MAIN_ANNOTATION_H
#define MAIN_ANNOTATION_H

#ifndef FLEXOS_VMEPT_COMP_ID
#error "FLEXOS_VMEPT_COMP_ID must be defined"
#endif
#ifndef FLEXOS_VMEPT_COMP_COUNT
#error "FLEXOS_VMEPT_COMP_COUNT must be defined"
#endif
#ifndef FLEXOS_VMEPT_APPCOMP
#error "FLEXOS_VMEPT_APPCOMP must be defined"
#endif

/*
 * Keep main in .text for every compartment image so VMEPT function-pointer
 * addresses match. Library VMs idle in ukboot before calling main.
 */
#define FLEXOS_VMEPT_MAIN_ANNOTATION

int main(int argc, char *argv[]) FLEXOS_VMEPT_MAIN_ANNOTATION;

#endif /* MAIN_ANNOTATION_H */
