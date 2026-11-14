#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../exec/elf_validate.h"

#define IMAGE_SIZE 0x1004u
#define CHECK_REJECT(label, ...) do { \
    uint8_t *image = make_image(); elf32_plan plan; __VA_ARGS__; \
    if (elf32_validate_image(image, IMAGE_SIZE, &plan)) { \
        fprintf(stderr, "FAIL: accepted %s\n", label); failures++; \
    } else { printf("PASS: rejected %s\n", label); } free(image); \
} while (0)

static int failures;
static uint8_t *make_image(void)
{
    uint8_t *bytes = (uint8_t *)calloc(1, IMAGE_SIZE);
    elf32_header *h = (elf32_header *)bytes;
    elf32_program *p;
    if (!bytes) { perror("calloc"); exit(2); }
    h->ident[0]=0x7f; h->ident[1]='E'; h->ident[2]='L'; h->ident[3]='F';
    h->ident[4]=1; h->ident[5]=1; h->ident[6]=1;
    h->type=2; h->machine=3; h->version=1; h->entry=0x40000000;
    h->phoff=sizeof(*h); h->ehsize=sizeof(*h); h->phentsize=sizeof(elf32_program); h->phnum=1;
    p=(elf32_program *)(bytes+h->phoff);
    p->type=1; p->offset=0x1000; p->vaddr=0x40000000; p->filesz=4;
    p->memsz=32; p->flags=5; p->align=0x1000;
    memcpy(bytes+0x1000,"CODE",4);
    return bytes;
}
static elf32_program *prog(uint8_t *bytes)
{ return (elf32_program *)(bytes+((elf32_header *)bytes)->phoff); }

int main(void)
{
    elf32_plan plan;
    uint8_t *good=make_image();
    if (!elf32_validate_image(good, IMAGE_SIZE, &plan) || plan.page_count != 5 ||
        plan.load_count != 1 || plan.entry != 0x40000000) {
        fprintf(stderr,"FAIL: valid ELF32 rejected or wrong load plan\n"); failures++;
    } else printf("PASS: valid ELF32 accepted with 1 load page + 4 stack pages\n");
    free(good);

    CHECK_REJECT("invalid ELF magic", image[0]=0);
    CHECK_REJECT("unsupported ELF class", image[4]=2);
    CHECK_REJECT("unsupported ELF endianness", image[5]=2);
    CHECK_REJECT("wrong machine architecture", ((elf32_header *)image)->machine=62);
    CHECK_REJECT("unsupported ELF type", ((elf32_header *)image)->type=3);
    { uint8_t *image=make_image(); elf32_plan p;
      if (elf32_validate_image(image,sizeof(elf32_header)-1,&p)) { fprintf(stderr,"FAIL: truncated ELF header accepted\n"); failures++; }
      else { printf("PASS: rejected truncated ELF header\n"); } free(image); }
    CHECK_REJECT("invalid program-header offset", ((elf32_header *)image)->phoff=0xfffffff0u);
    { uint8_t *image=make_image(); elf32_plan p;
      if (elf32_validate_image(image,80,&p)) { fprintf(stderr,"FAIL: truncated program-header table accepted\n"); failures++; }
      else { printf("PASS: rejected program-header table beyond file\n"); } free(image); }
    CHECK_REJECT("invalid program-header entry size", ((elf32_header *)image)->phentsize=31);
    CHECK_REJECT("p_filesz greater than p_memsz", prog(image)->filesz=33);
    CHECK_REJECT("segment file range beyond file", prog(image)->offset=IMAGE_SIZE-2);
    CHECK_REJECT("virtual address overflow/out of user range", prog(image)->vaddr=0xfffffff0u);
    CHECK_REJECT("invalid p_align congruence", prog(image)->align=0x1000, prog(image)->offset=0x1001);
    CHECK_REJECT("entry outside executable PT_LOAD", ((elf32_header *)image)->entry=0x40001000u);
    CHECK_REJECT("segment overlapping reserved stack", prog(image)->vaddr=0xbfffc000u);
    CHECK_REJECT("segment in kernel address space", prog(image)->vaddr=0xc0000000u);
    CHECK_REJECT("image exceeds page limit", prog(image)->memsz=0x1000000u);
    CHECK_REJECT("dynamic segment", ((elf32_header *)image)->phnum=2, ((elf32_program *)(image+sizeof(elf32_header)))[1].type=2);
    { uint8_t *image=make_image(); elf32_plan p; elf32_header *h=(elf32_header *)image;
      h->phnum=2; elf32_program *q=prog(image); q[1]=q[0]; q[1].vaddr+=0x100;
      if (elf32_validate_image(image,IMAGE_SIZE,&p)) { fprintf(stderr,"FAIL: overlapping PT_LOAD accepted\n"); failures++; }
      else { printf("PASS: rejected incompatible overlapping PT_LOAD pages\n"); } free(image); }
    { uint8_t *image=make_image(); elf32_plan p; elf32_program *q=prog(image);
      q->vaddr+=0x123; q->align=1; ((elf32_header *)image)->entry=q->vaddr;
      if (!elf32_validate_image(image,IMAGE_SIZE,&p)) { fprintf(stderr,"FAIL: valid non-page-aligned segment rejected\n"); failures++; }
      else { printf("PASS: accepted valid non-page-aligned segment\n"); } free(image); }

    if (failures) { fprintf(stderr,"%d ELF validation test(s) failed\n",failures); return 1; }
    puts("All ELF validation tests passed.");
    return 0;
}
