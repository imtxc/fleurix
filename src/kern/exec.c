#include <param.h>
#include <x86.h>
#include <proto.h>
#include <proc.h>
#include <unistd.h>
//
#include <page.h>
#include <vm.h>
//
#include <buf.h>
#include <conf.h>
#include <hd.h>
//
#include <tty.h>
//
#include <super.h>
#include <inode.h>
#include <file.h>
// 
#include <a.out.h>

int upush(uint *esp, char *buf, int len);
int upush_argv(uint *esp, char **argv);

/*
 * exec.c
 *
 * this file indicated the implementation of exec(char* path, char **argv).
 * exec() create a NEW address space, which overlapped the old forked address 
 * space, make it demand-reading.
 * */


/* initialize a new struct vm according to an a.out executable 
 * image.
 * returns NULL on fail.
 * TODO: ignored envp yet.
 * */
int do_exec(char *path, char **argv){
    struct inode *ip;
    struct buf *bp;
    struct ahead *ah;
    struct page *pg;
    struct vm *vm;
    struct vma *vp;
    uint bn, argc, esp;
    uint base, text, data, bss, heap;
    char *argv0, **uargv;

    ip = namei(path, 0);
    if (ip==NULL) 
        goto _badf;
    // read the first block of file to get the a.out header.
    bn = bmap(ip, 0, 0);
    if (bn == 0) {
        goto _badf;
    }
    bp = bread(ip->i_dev, bn);
    ah = (struct ahead*)bp->b_data;
    // check this a.out header.
    if (ah->a_magic != NMAGIC) 
        goto _badf;
    // 
    base = ah->a_entry - sizeof(struct ahead); // note: keep alignment
    text = ah->a_entry;
    data = text + ah->a_tsize;
    bss  = data + ah->a_dsize;
    heap = bss  + ah->a_bsize;
    // initialize all the VMAs
    vm = &cu->p_vm;
    vm->vm_entry = ah->a_entry;
    vma_init(&(vm->vm_text),  text,  ah->a_tsize, VMA_MMAP, ip, text-base);
    vma_init(&(vm->vm_data),  data,  ah->a_dsize, VMA_MMAP, ip, data-base);
    vma_init(&(vm->vm_bss),   bss,   ah->a_bsize, VMA_ZERO, NULL, NULL);
    vma_init(&(vm->vm_heap),  heap,  PAGE,        VMA_ZERO, NULL, NULL);
    vma_init(&(vm->vm_stack), VM_STACK, PAGE,     VMA_STACK|VMA_ZERO, NULL, NULL);
    // push arguments to the end of user stack, which always the same address.
    esp = VM_STACK;
    upush(&esp, path, strlen(path)+1);
    argv0 = esp;
    argc = upush_argv(&esp, argv) + 1;
    upush(&esp, &argv0, sizeof(uint));
    uargv = esp;
    upush(&esp, &uargv, sizeof(uint));
    upush(&esp, &argc, sizeof(uint));

    brelse(bp);
    unlk_ino(ip);
    // enter user mode
    _retu(vm->vm_entry, esp);
    return 0;

_badf:
    brelse(bp);
    unlk_ino(ip);
    return NULL;
}

int upush_argv(uint *esp, char **argv){
    uint arglen, argc, i;
    char *str, **uargv;

    if (argv==NULL) {
        return 0;
    }
    argc = 0;
    arglen = 0;
    for (i=0; (str=argv[i])!=NULL; i++) {
        arglen += strlen(str)+1;
        argc++;
    }
    arglen += sizeof(char*) * argc;
    vm_verify(*esp-arglen, arglen);
    uargv = *esp - arglen;
    for (i=0; i<argc; i++){
        str = argv[i];
        upush(esp, str, strlen(str)+1);
        uargv[i] = (char *) *esp;
    }
    *esp = uargv;
    return argc;
}

/* push one string into the user stack. returns the new esp */
int upush(uint *esp, char *buf, int len){
    vm_verify(*esp-len, len);
    *esp -= len;
    memcpy(*esp, buf, len);
    return 0;
}

/* ---------------------------------------------------- */

/* for debug */
int dump_ahead(struct ahead *ah){
    printf("sizeof(struct ahead): %x\n", sizeof(struct ahead));
    printf("a_magic: %x\n", ah->a_magic);
    printf("a_tsize: %x\n", ah->a_tsize);
    printf("a_dsize: %x\n", ah->a_dsize);
    printf("a_bsize: %x\n", ah->a_bsize);
    printf("a_syms: %x\n", ah->a_syms);
    printf("a_entry: %x\n", ah->a_entry);
    printf("a_trsize: %x\n", ah->a_trsize);
    printf("a_drsize: %x\n", ah->a_drsize);
}
