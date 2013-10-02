cmd_drivers/staging/usbip/stub_dev.o := mipsel-openwrt-linux-uclibc-gcc -Wp,-MD,drivers/staging/usbip/.stub_dev.o.d  -nostdinc -isystem /home/thunder/new/wusb/openwrt/staging_dir/toolchain-mipsel_dsp_gcc-4.6-linaro_uClibc-0.9.33.2/lib/gcc/mipsel-openwrt-linux-uclibc/4.6.4/include -I/home/thunder/new/wusb/openwrt/build_dir/target-mipsel_dsp_uClibc-0.9.33.2/linux-ramips_rt305x/linux-3.10.10/arch/mips/include -Iarch/mips/include/generated  -Iinclude -I/home/thunder/new/wusb/openwrt/build_dir/target-mipsel_dsp_uClibc-0.9.33.2/linux-ramips_rt305x/linux-3.10.10/arch/mips/include/uapi -Iarch/mips/include/generated/uapi -I/home/thunder/new/wusb/openwrt/build_dir/target-mipsel_dsp_uClibc-0.9.33.2/linux-ramips_rt305x/linux-3.10.10/include/uapi -Iinclude/generated/uapi -include /home/thunder/new/wusb/openwrt/build_dir/target-mipsel_dsp_uClibc-0.9.33.2/linux-ramips_rt305x/linux-3.10.10/include/linux/kconfig.h -D__KERNEL__ -DVMLINUX_LOAD_ADDRESS=0xffffffff80000000 -DDATAOFFSET=0 -Wall -Wundef -Wstrict-prototypes -Wno-trigraphs -fno-strict-aliasing -fno-common -Werror-implicit-function-declaration -Wno-format-security -fno-delete-null-pointer-checks -Os -fno-caller-saves -mno-check-zero-division -mabi=32 -G 0 -mno-abicalls -fno-pic -pipe -mno-branch-likely -msoft-float -ffreestanding -march=mips32r2 -Wa,-mips32r2 -Wa,--trap -I/home/thunder/new/wusb/openwrt/build_dir/target-mipsel_dsp_uClibc-0.9.33.2/linux-ramips_rt305x/linux-3.10.10/arch/mips/include/asm/mach-ralink -I/home/thunder/new/wusb/openwrt/build_dir/target-mipsel_dsp_uClibc-0.9.33.2/linux-ramips_rt305x/linux-3.10.10/arch/mips/include/asm/mach-ralink/rt305x -I/home/thunder/new/wusb/openwrt/build_dir/target-mipsel_dsp_uClibc-0.9.33.2/linux-ramips_rt305x/linux-3.10.10/arch/mips/include/asm/mach-generic -Wframe-larger-than=1024 -fno-stack-protector -Wno-unused-but-set-variable -fomit-frame-pointer -g -femit-struct-debug-baseonly -fno-var-tracking -Wdeclaration-after-statement -Wno-pointer-sign -fno-strict-overflow -fconserve-stack -DCC_HAVE_ASM_GOTO  -DMODULE -mno-long-calls  -D"KBUILD_STR(s)=\#s" -D"KBUILD_BASENAME=KBUILD_STR(stub_dev)"  -D"KBUILD_MODNAME=KBUILD_STR(usbip_host)" -c -o drivers/staging/usbip/stub_dev.o drivers/staging/usbip/stub_dev.c

source_drivers/staging/usbip/stub_dev.o := drivers/staging/usbip/stub_dev.c

deps_drivers/staging/usbip/stub_dev.o := \
  include/linux/device.h \
    $(wildcard include/config/debug/devres.h) \
    $(wildcard include/config/acpi.h) \
    $(wildcard include/config/pinctrl.h) \
    $(wildcard include/config/numa.h) \
    $(wildcard include/config/cma.h) \
    $(wildcard include/config/pm/sleep.h) \
    $(wildcard include/config/devtmpfs.h) \
    $(wildcard include/config/printk.h) \
    $(wildcard include/config/dynamic/debug.h) \
    $(wildcard include/config/sysfs/deprecated.h) \
  include/linux/ioport.h \
    $(wildcard include/config/memory/hotremove.h) \
  include/linux/compiler.h \
    $(wildcard include/config/sparse/rcu/pointer.h) \
    $(wildcard include/config/trace/branch/profiling.h) \
    $(wildcard include/config/profile/all/branches.h) \
    $(wildcard include/config/enable/must/check.h) \
    $(wildcard include/config/enable/warn/deprecated.h) \
    $(wildcard include/config/kprobes.h) \
  include/linux/compiler-gcc.h \
    $(wildcard include/config/arch/supports/optimized/inlining.h) \
    $(wildcard include/config/optimize/inlining.h) \
  include/linux/compiler-gcc4.h \
    $(wildcard include/config/arch/use/builtin/bswap.h) \
  include/linux/types.h \
    $(wildcard include/config/uid16.h) \
    $(wildcard include/config/lbdaf.h) \
    $(wildcard include/config/arch/dma/addr/t/64bit.h) \
    $(wildcard include/config/phys/addr/t/64bit.h) \
    $(wildcard include/config/64bit.h) \
  include/uapi/linux/types.h \
  /home/thunder/new/wusb/openwrt/build_dir/target-mipsel_dsp_uClibc-0.9.33.2/linux-ramips_rt305x/linux-3.10.10/arch/mips/include/asm/types.h \
    $(wildcard include/config/64bit/phys/addr.h) \
  include/asm-generic/int-ll64.h \
  include/uapi/asm-generic/int-ll64.h \
  /home/thunder/new/wusb/openwrt/build_dir/target-mipsel_dsp_uClibc-0.9.33.2/linux-ramips_rt305x/linux-3.10.10/arch/mips/include/uapi/asm/bitsperlong.h \
  include/asm-generic/bitsperlong.h \
  include/uapi/asm-generic/bitsperlong.h \
  /home/thunder/new/wusb/openwrt/build_dir/target-mipsel_dsp_uClibc-0.9.33.2/linux-ramips_rt305x/linux-3.10.10/arch/mips/include/uapi/asm/types.h \
  /home/thunder/new/wusb/openwrt/build_dir/target-mipsel_dsp_uClibc-0.9.33.2/linux-ramips_rt305x/linux-3.10.10/include/uapi/linux/posix_types.h \
  include/linux/stddef.h \
  include/uapi/linux/stddef.h \
  /home/thunder/new/wusb/openwrt/build_dir/target-mipsel_dsp_uClibc-0.9.33.2/linux-ramips_rt305x/linux-3.10.10/arch/mips/include/uapi/asm/posix_types.h \
  /home/thunder/new/wusb/openwrt/build_dir/target-mipsel_dsp_uClibc-0.9.33.2/linux-ramips_rt305x/linux-3.10.10/arch/mips/include/uapi/asm/sgidefs.h \
  /home/thunder/new/wusb/openwrt/build_dir/target-mipsel_dsp_uClibc-0.9.33.2/linux-ramips_rt305x/linux-3.10.10/include/uapi/asm-generic/posix_types.h \
  include/linux/kobject.h \
  include/linux/list.h \
    $(wildcard include/config/debug/list.h) \
  include/linux/poison.h \
    $(wildcard include/config/illegal/pointer/value.h) \
  /home/thunder/new/wusb/openwrt/build_dir/target-mipsel_dsp_uClibc-0.9.33.2/linux-ramips_rt305x/linux-3.10.10/include/uapi/linux/const.h \
  include/linux/sysfs.h \
    $(wildcard include/config/debug/lock/alloc.h) \
    $(wildcard include/config/sysfs.h) \
  include/linux/errno.h \
  include/uapi/linux/errno.h \
  /home/thunder/new/wusb/openwrt/build_dir/target-mipsel_dsp_uClibc-0.9.33.2/linux-ramips_rt305x/linux-3.10.10/arch/mips/include/asm/errno.h \
  /home/thunder/new/wusb/openwrt/build_dir/target-mipsel_dsp_uClibc-0.9.33.2/linux-ramips_rt305x/linux-3.10.10/arch/mips/include/uapi/asm/errno.h \
  /home/thunder/new/wusb/openwrt/build_dir/target-mipsel_dsp_uClibc-0.9.33.2/linux-ramips_rt305x/linux-3.10.10/include/uapi/asm-generic/errno-base.h \
  include/linux/lockdep.h \
    $(wildcard include/config/lockdep.h) \
    $(wildcard include/config/lock/stat.h) \
    $(wildcard include/config/trace/irqflags.h) \
    $(wildcard include/config/prove/locking.h) \
    $(wildcard include/config/prove/rcu.h) \
  include/linux/kobject_ns.h \
  include/linux/atomic.h \
    $(wildcard include/config/arch/has/atomic/or.h) \
    $(wildcard include/config/generic/atomic64.h) \
  /home/thunder/new/wusb/openwrt/build_dir/target-mipsel_dsp_uClibc-0.9.33.2/linux-ramips_rt305x/linux-3.10.10/arch/mips/include/asm/atomic.h \
  include/linux/irqflags.h \
    $(wildcard include/config/irqsoff/tracer.h) \
    $(wildcard include/config/preempt/tracer.h) \
    $(wildcard include/config/trace/irqflags/support.h) \
  include/linux/typecheck.h \
  /home/thunder/new/wusb/openwrt/build_dir/target-mipsel_dsp_uClibc-0.9.33.2/linux-ramips_rt305x/linux-3.10.10/arch/mips/include/asm/irqflags.h \
    $(wildcard include/config/cpu/mipsr2.h) \
    $(wildcard include/config/mips/mt/smtc.h) \
    $(wildcard include/config/irq/cpu.h) \
  include/linux/stringify.h \
  /home/thunder/new/wusb/openwrt/build_dir/target-mipsel_dsp_uClibc-0.9.33.2/linux-ramips_rt305x/linux-3.10.10/arch/mips/include/asm/hazards.h \
    $(wildcard include/config/cpu/cavium/octeon.h) \
    $(wildcard include/config/cpu/mipsr1.h) \
    $(wildcard include/config/mips/alchemy.h) \
    $(wildcard include/config/cpu/bmips.h) \
    $(wildcard include/config/cpu/loongson2.h) \
    $(wildcard include/config/cpu/r10000.h) \
    $(wildcard include/config/cpu/r5500.h) \
    $(wildcard include/config/cpu/xlr.h) \
    $(wildcard include/config/cpu/sb1.h) \
  /home/thunder/new/wusb/openwrt/build_dir/target-mipsel_dsp_uClibc-0.9.33.2/linux-ramips_rt305x/linux-3.10.10/arch/mips/include/asm/barrier.h \
    $(wildcard include/config/cpu/has/sync.h) \
    $(wildcard include/config/sgi/ip28.h) \
    $(wildcard include/config/cpu/has/wb.h) \
    $(wildcard include/config/weak/ordering.h) \
    $(wildcard include/config/smp.h) \
    $(wildcard include/config/weak/reordering/beyond/llsc.h) \
  /home/thunder/new/wusb/openwrt/build_dir/target-mipsel_dsp_uClibc-0.9.33.2/linux-ramips_rt305x/linux-3.10.10/arch/mips/include/asm/addrspace.h \
    $(wildcard include/config/cpu/r8000.h) \
  /home/thunder/new/wusb/openwrt/build_dir/target-mipsel_dsp_uClibc-0.9.33.2/linux-ramips_rt305x/linux-3.10.10/arch/mips/include/asm/mach-generic/spaces.h \
    $(wildcard include/config/32bit.h) \
    $(wildcard include/config/kvm/guest.h) \
    $(wildcard include/config/dma/noncoherent.h) \
  /home/thunder/new/wusb/openwrt/build_dir/target-mipsel_dsp_uClibc-0.9.33.2/linux-ramips_rt305x/linux-3.10.10/arch/mips/include/asm/cpu-features.h \
    $(wildcard include/config/sys/supports/micromips.h) \
    $(wildcard include/config/cpu/mipsr2/irq/vi.h) \
    $(wildcard include/config/cpu/mipsr2/irq/ei.h) \
  /home/thunder/new/wusb/openwrt/build_dir/target-mipsel_dsp_uClibc-0.9.33.2/linux-ramips_rt305x/linux-3.10.10/arch/mips/include/asm/cpu.h \
  /home/thunder/new/wusb/openwrt/build_dir/target-mipsel_dsp_uClibc-0.9.33.2/linux-ramips_rt305x/linux-3.10.10/arch/mips/include/asm/cpu-info.h \
    $(wildcard include/config/mips/mt/smp.h) \
  /home/thunder/new/wusb/openwrt/build_dir/target-mipsel_dsp_uClibc-0.9.33.2/linux-ramips_rt305x/linux-3.10.10/arch/mips/include/asm/cache.h \
    $(wildcard include/config/mips/l1/cache/shift.h) \
  /home/thunder/new/wusb/openwrt/build_dir/target-mipsel_dsp_uClibc-0.9.33.2/linux-ramips_rt305x/linux-3.10.10/arch/mips/include/asm/mach-generic/kmalloc.h \
    $(wildcard include/config/dma/coherent.h) \
  /home/thunder/new/wusb/openwrt/build_dir/target-mipsel_dsp_uClibc-0.9.33.2/linux-ramips_rt305x/linux-3.10.10/arch/mips/include/asm/mach-ralink/rt305x/cpu-feature-overrides.h \
  /home/thunder/new/wusb/openwrt/build_dir/target-mipsel_dsp_uClibc-0.9.33.2/linux-ramips_rt305x/linux-3.10.10/arch/mips/include/asm/cmpxchg.h \
  include/linux/bug.h \
    $(wildcard include/config/generic/bug.h) \
  /home/thunder/new/wusb/openwrt/build_dir/target-mipsel_dsp_uClibc-0.9.33.2/linux-ramips_rt305x/linux-3.10.10/arch/mips/include/asm/bug.h \
    $(wildcard include/config/bug.h) \
  /home/thunder/new/wusb/openwrt/build_dir/target-mipsel_dsp_uClibc-0.9.33.2/linux-ramips_rt305x/linux-3.10.10/arch/mips/include/asm/break.h \
  /home/thunder/new/wusb/openwrt/build_dir/target-mipsel_dsp_uClibc-0.9.33.2/linux-ramips_rt305x/linux-3.10.10/arch/mips/include/uapi/asm/break.h \
  include/asm-generic/bug.h \
    $(wildcard include/config/generic/bug/relative/pointers.h) \
    $(wildcard include/config/debug/bugverbose.h) \
  include/linux/kernel.h \
    $(wildcard include/config/preempt/voluntary.h) \
    $(wildcard include/config/debug/atomic/sleep.h) \
    $(wildcard include/config/ring/buffer.h) \
    $(wildcard include/config/tracing.h) \
    $(wildcard include/config/ftrace/mcount/record.h) \
  /home/thunder/new/wusb/openwrt/staging_dir/toolchain-mipsel_dsp_gcc-4.6-linaro_uClibc-0.9.33.2/lib/gcc/mipsel-openwrt-linux-uclibc/4.6.4/include/stdarg.h \
  include/linux/linkage.h \
  include/linux/export.h \
    $(wildcard include/config/have/underscore/symbol/prefix.h) \
    $(wildcard include/config/modules.h) \
    $(wildcard include/config/modversions.h) \
    $(wildcard include/config/unused/symbols.h) \
  /home/thunder/new/wusb/openwrt/build_dir/target-mipsel_dsp_uClibc-0.9.33.2/linux-ramips_rt305x/linux-3.10.10/arch/mips/include/asm/linkage.h \
  include/linux/bitops.h \
  /home/thunder/new/wusb/openwrt/build_dir/target-mipsel_dsp_uClibc-0.9.33.2/linux-ramips_rt305x/linux-3.10.10/arch/mips/include/asm/bitops.h \
  /home/thunder/new/wusb/openwrt/build_dir/target-mipsel_dsp_uClibc-0.9.33.2/linux-ramips_rt305x/linux-3.10.10/arch/mips/include/uapi/asm/byteorder.h \
  include/linux/byteorder/little_endian.h \
  include/uapi/linux/byteorder/little_endian.h \
  include/linux/swab.h \
  include/uapi/linux/swab.h \
  /home/thunder/new/wusb/openwrt/build_dir/target-mipsel_dsp_uClibc-0.9.33.2/linux-ramips_rt305x/linux-3.10.10/arch/mips/include/uapi/asm/swab.h \
  include/linux/byteorder/generic.h \
  /home/thunder/new/wusb/openwrt/build_dir/target-mipsel_dsp_uClibc-0.9.33.2/linux-ramips_rt305x/linux-3.10.10/arch/mips/include/asm/war.h \
    $(wildcard include/config/cpu/r4000/workarounds.h) \
    $(wildcard include/config/cpu/r4400/workarounds.h) \
    $(wildcard include/config/cpu/daddi/workarounds.h) \
  /home/thunder/new/wusb/openwrt/build_dir/target-mipsel_dsp_uClibc-0.9.33.2/linux-ramips_rt305x/linux-3.10.10/arch/mips/include/asm/mach-ralink/war.h \
  include/asm-generic/bitops/non-atomic.h \
  include/asm-generic/bitops/fls64.h \
  include/asm-generic/bitops/ffz.h \
  include/asm-generic/bitops/find.h \
    $(wildcard include/config/generic/find/first/bit.h) \
  include/asm-generic/bitops/sched.h \
  /home/thunder/new/wusb/openwrt/build_dir/target-mipsel_dsp_uClibc-0.9.33.2/linux-ramips_rt305x/linux-3.10.10/arch/mips/include/asm/arch_hweight.h \
  include/asm-generic/bitops/arch_hweight.h \
  include/asm-generic/bitops/const_hweight.h \
  include/asm-generic/bitops/le.h \
  include/asm-generic/bitops/ext2-atomic.h \
  include/linux/log2.h \
    $(wildcard include/config/arch/has/ilog2/u32.h) \
    $(wildcard include/config/arch/has/ilog2/u64.h) \
  include/linux/printk.h \
    $(wildcard include/config/early/printk.h) \
  include/linux/init.h \
    $(wildcard include/config/broken/rodata.h) \
  include/linux/kern_levels.h \
  include/linux/dynamic_debug.h \
  include/linux/string.h \
    $(wildcard include/config/binary/printf.h) \
  include/uapi/linux/string.h \
  /home/thunder/new/wusb/openwrt/build_dir/target-mipsel_dsp_uClibc-0.9.33.2/linux-ramips_rt305x/linux-3.10.10/arch/mips/include/asm/string.h \
    $(wildcard include/config/cpu/r3000.h) \
  include/uapi/linux/kernel.h \
  /home/thunder/new/wusb/openwrt/build_dir/target-mipsel_dsp_uClibc-0.9.33.2/linux-ramips_rt305x/linux-3.10.10/include/uapi/linux/sysinfo.h \
  /home/thunder/new/wusb/openwrt/build_dir/target-mipsel_dsp_uClibc-0.9.33.2/linux-ramips_rt305x/linux-3.10.10/arch/mips/include/asm/div64.h \
  include/asm-generic/div64.h \
  include/asm-generic/cmpxchg-local.h \
  include/asm-generic/atomic-long.h \
  include/asm-generic/atomic64.h \
  include/linux/spinlock.h \
    $(wildcard include/config/debug/spinlock.h) \
    $(wildcard include/config/generic/lockbreak.h) \
    $(wildcard include/config/preempt.h) \
  include/linux/preempt.h \
    $(wildcard include/config/debug/preempt.h) \
    $(wildcard include/config/context/tracking.h) \
    $(wildcard include/config/preempt/count.h) \
    $(wildcard include/config/preempt/notifiers.h) \
  include/linux/thread_info.h \
    $(wildcard include/config/compat.h) \
    $(wildcard include/config/debug/stack/usage.h) \
  /home/thunder/new/wusb/openwrt/build_dir/target-mipsel_dsp_uClibc-0.9.33.2/linux-ramips_rt305x/linux-3.10.10/arch/mips/include/asm/thread_info.h \
    $(wildcard include/config/page/size/4kb.h) \
    $(wildcard include/config/page/size/8kb.h) \
    $(wildcard include/config/page/size/16kb.h) \
    $(wildcard include/config/page/size/32kb.h) \
    $(wildcard include/config/page/size/64kb.h) \
  /home/thunder/new/wusb/openwrt/build_dir/target-mipsel_dsp_uClibc-0.9.33.2/linux-ramips_rt305x/linux-3.10.10/arch/mips/include/asm/processor.h \
    $(wildcard include/config/cavium/octeon/cvmseg/size.h) \
    $(wildcard include/config/mips/mt/fpaff.h) \
    $(wildcard include/config/cpu/has/prefetch.h) \
  include/linux/cpumask.h \
    $(wildcard include/config/cpumask/offstack.h) \
    $(wildcard include/config/hotplug/cpu.h) \
    $(wildcard include/config/debug/per/cpu/maps.h) \
    $(wildcard include/config/disable/obsolete/cpumask/functions.h) \
  include/linux/threads.h \
    $(wildcard include/config/nr/cpus.h) \
    $(wildcard include/config/base/small.h) \
  include/linux/bitmap.h \
  /home/thunder/new/wusb/openwrt/build_dir/target-mipsel_dsp_uClibc-0.9.33.2/linux-ramips_rt305x/linux-3.10.10/arch/mips/include/uapi/asm/cachectl.h \
  /home/thunder/new/wusb/openwrt/build_dir/target-mipsel_dsp_uClibc-0.9.33.2/linux-ramips_rt305x/linux-3.10.10/arch/mips/include/asm/mipsregs.h \
    $(wildcard include/config/cpu/vr41xx.h) \
    $(wildcard include/config/mips/huge/tlb/support.h) \
    $(wildcard include/config/cpu/micromips.h) \
  /home/thunder/new/wusb/openwrt/build_dir/target-mipsel_dsp_uClibc-0.9.33.2/linux-ramips_rt305x/linux-3.10.10/arch/mips/include/asm/prefetch.h \
  include/linux/bottom_half.h \
  include/linux/spinlock_types.h \
  include/linux/spinlock_types_up.h \
  include/linux/rwlock_types.h \
  include/linux/spinlock_up.h \
  include/linux/rwlock.h \
  include/linux/spinlock_api_up.h \
  include/linux/kref.h \
  include/linux/mutex.h \
    $(wildcard include/config/debug/mutexes.h) \
    $(wildcard include/config/mutex/spin/on/owner.h) \
    $(wildcard include/config/have/arch/mutex/cpu/relax.h) \
  include/linux/wait.h \
  /home/thunder/new/wusb/openwrt/build_dir/target-mipsel_dsp_uClibc-0.9.33.2/linux-ramips_rt305x/linux-3.10.10/arch/mips/include/asm/current.h \
  include/asm-generic/current.h \
  include/uapi/linux/wait.h \
  include/linux/klist.h \
  include/linux/pinctrl/devinfo.h \
  include/linux/pm.h \
    $(wildcard include/config/vt/console/sleep.h) \
    $(wildcard include/config/pm.h) \
    $(wildcard include/config/pm/runtime.h) \
    $(wildcard include/config/pm/clk.h) \
    $(wildcard include/config/pm/generic/domains.h) \
  include/linux/workqueue.h \
    $(wildcard include/config/debug/objects/work.h) \
    $(wildcard include/config/freezer.h) \
  include/linux/timer.h \
    $(wildcard include/config/timer/stats.h) \
    $(wildcard include/config/debug/objects/timers.h) \
  include/linux/ktime.h \
    $(wildcard include/config/ktime/scalar.h) \
  include/linux/time.h \
    $(wildcard include/config/arch/uses/gettimeoffset.h) \
  include/linux/cache.h \
    $(wildcard include/config/arch/has/cache/line/size.h) \
  include/linux/seqlock.h \
  include/linux/math64.h \
  include/uapi/linux/time.h \
  include/linux/jiffies.h \
  include/linux/timex.h \
  include/uapi/linux/timex.h \
  /home/thunder/new/wusb/openwrt/build_dir/target-mipsel_dsp_uClibc-0.9.33.2/linux-ramips_rt305x/linux-3.10.10/include/uapi/linux/param.h \
  /home/thunder/new/wusb/openwrt/build_dir/target-mipsel_dsp_uClibc-0.9.33.2/linux-ramips_rt305x/linux-3.10.10/arch/mips/include/uapi/asm/param.h \
  include/asm-generic/param.h \
    $(wildcard include/config/hz.h) \
  include/uapi/asm-generic/param.h \
  /home/thunder/new/wusb/openwrt/build_dir/target-mipsel_dsp_uClibc-0.9.33.2/linux-ramips_rt305x/linux-3.10.10/arch/mips/include/asm/timex.h \
  include/linux/debugobjects.h \
    $(wildcard include/config/debug/objects.h) \
    $(wildcard include/config/debug/objects/free.h) \
  include/linux/completion.h \
  include/linux/ratelimit.h \
  include/linux/uidgid.h \
    $(wildcard include/config/uidgid/strict/type/checks.h) \
    $(wildcard include/config/user/ns.h) \
  include/linux/highuid.h \
  /home/thunder/new/wusb/openwrt/build_dir/target-mipsel_dsp_uClibc-0.9.33.2/linux-ramips_rt305x/linux-3.10.10/arch/mips/include/asm/device.h \
  include/linux/pm_wakeup.h \
  include/linux/file.h \
  include/linux/kthread.h \
  include/linux/err.h \
  include/linux/sched.h \
    $(wildcard include/config/sched/debug.h) \
    $(wildcard include/config/no/hz/common.h) \
    $(wildcard include/config/lockup/detector.h) \
    $(wildcard include/config/mmu.h) \
    $(wildcard include/config/core/dump/default/elf/headers.h) \
    $(wildcard include/config/sched/autogroup.h) \
    $(wildcard include/config/virt/cpu/accounting/native.h) \
    $(wildcard include/config/bsd/process/acct.h) \
    $(wildcard include/config/taskstats.h) \
    $(wildcard include/config/audit.h) \
    $(wildcard include/config/cgroups.h) \
    $(wildcard include/config/inotify/user.h) \
    $(wildcard include/config/fanotify.h) \
    $(wildcard include/config/epoll.h) \
    $(wildcard include/config/posix/mqueue.h) \
    $(wildcard include/config/keys.h) \
    $(wildcard include/config/perf/events.h) \
    $(wildcard include/config/schedstats.h) \
    $(wildcard include/config/task/delay/acct.h) \
    $(wildcard include/config/fair/group/sched.h) \
    $(wildcard include/config/rt/group/sched.h) \
    $(wildcard include/config/cgroup/sched.h) \
    $(wildcard include/config/blk/dev/io/trace.h) \
    $(wildcard include/config/preempt/rcu.h) \
    $(wildcard include/config/tree/preempt/rcu.h) \
    $(wildcard include/config/rcu/boost.h) \
    $(wildcard include/config/compat/brk.h) \
    $(wildcard include/config/cc/stackprotector.h) \
    $(wildcard include/config/virt/cpu/accounting/gen.h) \
    $(wildcard include/config/sysvipc.h) \
    $(wildcard include/config/detect/hung/task.h) \
    $(wildcard include/config/auditsyscall.h) \
    $(wildcard include/config/rt/mutexes.h) \
    $(wildcard include/config/block.h) \
    $(wildcard include/config/task/xacct.h) \
    $(wildcard include/config/cpusets.h) \
    $(wildcard include/config/futex.h) \
    $(wildcard include/config/numa/balancing.h) \
    $(wildcard include/config/fault/injection.h) \
    $(wildcard include/config/latencytop.h) \
    $(wildcard include/config/function/graph/tracer.h) \
    $(wildcard include/config/memcg.h) \
    $(wildcard include/config/have/hw/breakpoint.h) \
    $(wildcard include/config/uprobes.h) \
    $(wildcard include/config/bcache.h) \
    $(wildcard include/config/have/unstable/sched/clock.h) \
    $(wildcard include/config/irq/time/accounting.h) \
    $(wildcard include/config/no/hz/full.h) \
    $(wildcard include/config/proc/fs.h) \
    $(wildcard include/config/stack/growsup.h) \
    $(wildcard include/config/mm/owner.h) \
  include/uapi/linux/sched.h \
  include/linux/capability.h \
  include/uapi/linux/capability.h \
  include/linux/rbtree.h \
  include/linux/nodemask.h \
    $(wildcard include/config/highmem.h) \
    $(wildcard include/config/movable/node.h) \
  include/linux/numa.h \
    $(wildcard include/config/nodes/shift.h) \
  include/linux/mm_types.h \
    $(wildcard include/config/split/ptlock/cpus.h) \
    $(wildcard include/config/have/cmpxchg/double.h) \
    $(wildcard include/config/have/aligned/struct/page.h) \
    $(wildcard include/config/want/page/debug/flags.h) \
    $(wildcard include/config/kmemcheck.h) \
    $(wildcard include/config/aio.h) \
    $(wildcard include/config/mmu/notifier.h) \
    $(wildcard include/config/transparent/hugepage.h) \
  include/linux/auxvec.h \
  include/uapi/linux/auxvec.h \
  /home/thunder/new/wusb/openwrt/build_dir/target-mipsel_dsp_uClibc-0.9.33.2/linux-ramips_rt305x/linux-3.10.10/arch/mips/include/uapi/asm/auxvec.h \
  include/linux/rwsem.h \
    $(wildcard include/config/rwsem/generic/spinlock.h) \
  include/linux/rwsem-spinlock.h \
  include/linux/page-debug-flags.h \
    $(wildcard include/config/page/poisoning.h) \
    $(wildcard include/config/page/guard.h) \
    $(wildcard include/config/page/debug/something/else.h) \
  include/linux/uprobes.h \
    $(wildcard include/config/arch/supports/uprobes.h) \
  include/linux/page-flags-layout.h \
    $(wildcard include/config/sparsemem.h) \
    $(wildcard include/config/sparsemem/vmemmap.h) \
  include/generated/bounds.h \
  /home/thunder/new/wusb/openwrt/build_dir/target-mipsel_dsp_uClibc-0.9.33.2/linux-ramips_rt305x/linux-3.10.10/arch/mips/include/asm/page.h \
    $(wildcard include/config/cpu/mips32.h) \
    $(wildcard include/config/flatmem.h) \
    $(wildcard include/config/need/multiple/nodes.h) \
  include/linux/pfn.h \
  /home/thunder/new/wusb/openwrt/build_dir/target-mipsel_dsp_uClibc-0.9.33.2/linux-ramips_rt305x/linux-3.10.10/arch/mips/include/asm/io.h \
    $(wildcard include/config/pci.h) \
  include/asm-generic/iomap.h \
    $(wildcard include/config/has/ioport.h) \
    $(wildcard include/config/generic/iomap.h) \
  include/asm-generic/pci_iomap.h \
    $(wildcard include/config/no/generic/pci/ioport/map.h) \
    $(wildcard include/config/generic/pci/iomap.h) \
  /home/thunder/new/wusb/openwrt/build_dir/target-mipsel_dsp_uClibc-0.9.33.2/linux-ramips_rt305x/linux-3.10.10/arch/mips/include/asm/pgtable-bits.h \
    $(wildcard include/config/cpu/tx39xx.h) \
  /home/thunder/new/wusb/openwrt/build_dir/target-mipsel_dsp_uClibc-0.9.33.2/linux-ramips_rt305x/linux-3.10.10/arch/mips/include/asm/mach-generic/ioremap.h \
  /home/thunder/new/wusb/openwrt/build_dir/target-mipsel_dsp_uClibc-0.9.33.2/linux-ramips_rt305x/linux-3.10.10/arch/mips/include/asm/mach-generic/mangle-port.h \
    $(wildcard include/config/swap/io/space.h) \
  include/asm-generic/memory_model.h \
    $(wildcard include/config/discontigmem.h) \
  include/asm-generic/getorder.h \
  /home/thunder/new/wusb/openwrt/build_dir/target-mipsel_dsp_uClibc-0.9.33.2/linux-ramips_rt305x/linux-3.10.10/arch/mips/include/asm/mmu.h \
  /home/thunder/new/wusb/openwrt/build_dir/target-mipsel_dsp_uClibc-0.9.33.2/linux-ramips_rt305x/linux-3.10.10/arch/mips/include/asm/ptrace.h \
    $(wildcard include/config/cpu/has/smartmips.h) \
  /home/thunder/new/wusb/openwrt/build_dir/target-mipsel_dsp_uClibc-0.9.33.2/linux-ramips_rt305x/linux-3.10.10/arch/mips/include/asm/isadep.h \
  /home/thunder/new/wusb/openwrt/build_dir/target-mipsel_dsp_uClibc-0.9.33.2/linux-ramips_rt305x/linux-3.10.10/arch/mips/include/uapi/asm/ptrace.h \
  /home/thunder/new/wusb/openwrt/build_dir/target-mipsel_dsp_uClibc-0.9.33.2/linux-ramips_rt305x/linux-3.10.10/arch/mips/include/asm/cputime.h \
  include/asm-generic/cputime.h \
    $(wildcard include/config/virt/cpu/accounting.h) \
  include/asm-generic/cputime_jiffies.h \
  include/linux/smp.h \
    $(wildcard include/config/use/generic/smp/helpers.h) \
  include/linux/sem.h \
  include/linux/rcupdate.h \
    $(wildcard include/config/rcu/torture/test.h) \
    $(wildcard include/config/tree/rcu.h) \
    $(wildcard include/config/rcu/trace.h) \
    $(wildcard include/config/rcu/user/qs.h) \
    $(wildcard include/config/tiny/rcu.h) \
    $(wildcard include/config/tiny/preempt/rcu.h) \
    $(wildcard include/config/debug/objects/rcu/head.h) \
    $(wildcard include/config/rcu/nocb/cpu.h) \
  include/linux/rcutiny.h \
  include/uapi/linux/sem.h \
  include/linux/ipc.h \
  include/uapi/linux/ipc.h \
  /home/thunder/new/wusb/openwrt/build_dir/target-mipsel_dsp_uClibc-0.9.33.2/linux-ramips_rt305x/linux-3.10.10/arch/mips/include/uapi/asm/ipcbuf.h \
  /home/thunder/new/wusb/openwrt/build_dir/target-mipsel_dsp_uClibc-0.9.33.2/linux-ramips_rt305x/linux-3.10.10/include/uapi/asm-generic/ipcbuf.h \
  /home/thunder/new/wusb/openwrt/build_dir/target-mipsel_dsp_uClibc-0.9.33.2/linux-ramips_rt305x/linux-3.10.10/arch/mips/include/uapi/asm/sembuf.h \
  include/linux/signal.h \
    $(wildcard include/config/old/sigaction.h) \
  include/uapi/linux/signal.h \
  /home/thunder/new/wusb/openwrt/build_dir/target-mipsel_dsp_uClibc-0.9.33.2/linux-ramips_rt305x/linux-3.10.10/arch/mips/include/asm/signal.h \
    $(wildcard include/config/trad/signals.h) \
  /home/thunder/new/wusb/openwrt/build_dir/target-mipsel_dsp_uClibc-0.9.33.2/linux-ramips_rt305x/linux-3.10.10/arch/mips/include/uapi/asm/signal.h \
  /home/thunder/new/wusb/openwrt/build_dir/target-mipsel_dsp_uClibc-0.9.33.2/linux-ramips_rt305x/linux-3.10.10/include/uapi/asm-generic/signal-defs.h \
  /home/thunder/new/wusb/openwrt/build_dir/target-mipsel_dsp_uClibc-0.9.33.2/linux-ramips_rt305x/linux-3.10.10/arch/mips/include/asm/sigcontext.h \
  /home/thunder/new/wusb/openwrt/build_dir/target-mipsel_dsp_uClibc-0.9.33.2/linux-ramips_rt305x/linux-3.10.10/arch/mips/include/uapi/asm/sigcontext.h \
  /home/thunder/new/wusb/openwrt/build_dir/target-mipsel_dsp_uClibc-0.9.33.2/linux-ramips_rt305x/linux-3.10.10/arch/mips/include/asm/siginfo.h \
  /home/thunder/new/wusb/openwrt/build_dir/target-mipsel_dsp_uClibc-0.9.33.2/linux-ramips_rt305x/linux-3.10.10/arch/mips/include/uapi/asm/siginfo.h \
  include/asm-generic/siginfo.h \
  include/uapi/asm-generic/siginfo.h \
  include/linux/pid.h \
  include/linux/percpu.h \
    $(wildcard include/config/need/per/cpu/embed/first/chunk.h) \
    $(wildcard include/config/need/per/cpu/page/first/chunk.h) \
    $(wildcard include/config/have/setup/per/cpu/area.h) \
  /home/thunder/new/wusb/openwrt/build_dir/target-mipsel_dsp_uClibc-0.9.33.2/linux-ramips_rt305x/linux-3.10.10/arch/mips/include/asm/percpu.h \
  include/asm-generic/percpu.h \
  include/linux/percpu-defs.h \
    $(wildcard include/config/debug/force/weak/per/cpu.h) \
  include/linux/topology.h \
    $(wildcard include/config/sched/smt.h) \
    $(wildcard include/config/sched/mc.h) \
    $(wildcard include/config/sched/book.h) \
    $(wildcard include/config/use/percpu/numa/node/id.h) \
    $(wildcard include/config/have/memoryless/nodes.h) \
  include/linux/mmzone.h \
    $(wildcard include/config/force/max/zoneorder.h) \
    $(wildcard include/config/memory/isolation.h) \
    $(wildcard include/config/zone/dma.h) \
    $(wildcard include/config/zone/dma32.h) \
    $(wildcard include/config/compaction.h) \
    $(wildcard include/config/memory/hotplug.h) \
    $(wildcard include/config/have/memblock/node/map.h) \
    $(wildcard include/config/flat/node/mem/map.h) \
    $(wildcard include/config/no/bootmem.h) \
    $(wildcard include/config/have/memory/present.h) \
    $(wildcard include/config/need/node/memmap/size.h) \
    $(wildcard include/config/have/arch/early/pfn/to/nid.h) \
    $(wildcard include/config/sparsemem/extreme.h) \
    $(wildcard include/config/have/arch/pfn/valid.h) \
    $(wildcard include/config/nodes/span/other/nodes.h) \
    $(wildcard include/config/holes/in/zone.h) \
    $(wildcard include/config/arch/has/holes/memorymodel.h) \
  include/linux/pageblock-flags.h \
    $(wildcard include/config/hugetlb/page.h) \
    $(wildcard include/config/hugetlb/page/size/variable.h) \
  include/linux/memory_hotplug.h \
    $(wildcard include/config/have/arch/nodedata/extension.h) \
    $(wildcard include/config/have/bootmem/info/node.h) \
  include/linux/notifier.h \
  include/linux/srcu.h \
  /home/thunder/new/wusb/openwrt/build_dir/target-mipsel_dsp_uClibc-0.9.33.2/linux-ramips_rt305x/linux-3.10.10/arch/mips/include/asm/topology.h \
  /home/thunder/new/wusb/openwrt/build_dir/target-mipsel_dsp_uClibc-0.9.33.2/linux-ramips_rt305x/linux-3.10.10/arch/mips/include/asm/mach-generic/topology.h \
  include/asm-generic/topology.h \
  include/linux/proportions.h \
  include/linux/percpu_counter.h \
  include/linux/seccomp.h \
    $(wildcard include/config/seccomp.h) \
    $(wildcard include/config/seccomp/filter.h) \
  include/uapi/linux/seccomp.h \
  include/linux/rculist.h \
  include/linux/rtmutex.h \
    $(wildcard include/config/debug/rt/mutexes.h) \
  include/linux/plist.h \
    $(wildcard include/config/debug/pi/list.h) \
  include/linux/resource.h \
  include/uapi/linux/resource.h \
  /home/thunder/new/wusb/openwrt/build_dir/target-mipsel_dsp_uClibc-0.9.33.2/linux-ramips_rt305x/linux-3.10.10/arch/mips/include/uapi/asm/resource.h \
  include/asm-generic/resource.h \
  include/uapi/asm-generic/resource.h \
  include/linux/hrtimer.h \
    $(wildcard include/config/high/res/timers.h) \
    $(wildcard include/config/timerfd.h) \
  include/linux/timerqueue.h \
  include/linux/task_io_accounting.h \
    $(wildcard include/config/task/io/accounting.h) \
  include/linux/latencytop.h \
  include/linux/cred.h \
    $(wildcard include/config/debug/credentials.h) \
    $(wildcard include/config/security.h) \
  include/linux/key.h \
    $(wildcard include/config/sysctl.h) \
  include/linux/sysctl.h \
  include/uapi/linux/sysctl.h \
  include/linux/selinux.h \
    $(wildcard include/config/security/selinux.h) \
  include/linux/llist.h \
    $(wildcard include/config/arch/have/nmi/safe/cmpxchg.h) \
  include/linux/gfp.h \
  include/linux/mmdebug.h \
    $(wildcard include/config/debug/vm.h) \
    $(wildcard include/config/debug/virtual.h) \
  include/linux/module.h \
    $(wildcard include/config/module/stripped.h) \
    $(wildcard include/config/module/sig.h) \
    $(wildcard include/config/kallsyms.h) \
    $(wildcard include/config/tracepoints.h) \
    $(wildcard include/config/event/tracing.h) \
    $(wildcard include/config/module/unload.h) \
    $(wildcard include/config/constructors.h) \
    $(wildcard include/config/debug/set/module/ronx.h) \
  include/linux/stat.h \
  /home/thunder/new/wusb/openwrt/build_dir/target-mipsel_dsp_uClibc-0.9.33.2/linux-ramips_rt305x/linux-3.10.10/arch/mips/include/uapi/asm/stat.h \
  include/uapi/linux/stat.h \
  include/linux/kmod.h \
  include/linux/elf.h \
  /home/thunder/new/wusb/openwrt/build_dir/target-mipsel_dsp_uClibc-0.9.33.2/linux-ramips_rt305x/linux-3.10.10/arch/mips/include/asm/elf.h \
    $(wildcard include/config/mips32/n32.h) \
    $(wildcard include/config/mips32/o32.h) \
    $(wildcard include/config/mips32/compat.h) \
  include/uapi/linux/elf.h \
  /home/thunder/new/wusb/openwrt/build_dir/target-mipsel_dsp_uClibc-0.9.33.2/linux-ramips_rt305x/linux-3.10.10/include/uapi/linux/elf-em.h \
  include/linux/moduleparam.h \
    $(wildcard include/config/alpha.h) \
    $(wildcard include/config/ia64.h) \
    $(wildcard include/config/ppc64.h) \
  include/linux/tracepoint.h \
  include/linux/static_key.h \
  include/linux/jump_label.h \
    $(wildcard include/config/jump/label.h) \
  /home/thunder/new/wusb/openwrt/build_dir/target-mipsel_dsp_uClibc-0.9.33.2/linux-ramips_rt305x/linux-3.10.10/arch/mips/include/asm/module.h \
    $(wildcard include/config/cpu/mips32/r1.h) \
    $(wildcard include/config/cpu/mips32/r2.h) \
    $(wildcard include/config/cpu/mips64/r1.h) \
    $(wildcard include/config/cpu/mips64/r2.h) \
    $(wildcard include/config/cpu/r4300.h) \
    $(wildcard include/config/cpu/r4x00.h) \
    $(wildcard include/config/cpu/tx49xx.h) \
    $(wildcard include/config/cpu/r5000.h) \
    $(wildcard include/config/cpu/r5432.h) \
    $(wildcard include/config/cpu/r6000.h) \
    $(wildcard include/config/cpu/nevada.h) \
    $(wildcard include/config/cpu/rm7000.h) \
    $(wildcard include/config/cpu/loongson1.h) \
    $(wildcard include/config/cpu/xlp.h) \
  /home/thunder/new/wusb/openwrt/build_dir/target-mipsel_dsp_uClibc-0.9.33.2/linux-ramips_rt305x/linux-3.10.10/arch/mips/include/asm/uaccess.h \
  drivers/staging/usbip/usbip_common.h \
  include/linux/interrupt.h \
    $(wildcard include/config/generic/hardirqs.h) \
    $(wildcard include/config/irq/forced/threading.h) \
    $(wildcard include/config/generic/irq/probe.h) \
  include/linux/irqreturn.h \
  include/linux/irqnr.h \
  include/uapi/linux/irqnr.h \
  include/linux/hardirq.h \
  include/linux/ftrace_irq.h \
    $(wildcard include/config/ftrace/nmi/enter.h) \
  include/linux/vtime.h \
  /home/thunder/new/wusb/openwrt/build_dir/target-mipsel_dsp_uClibc-0.9.33.2/linux-ramips_rt305x/linux-3.10.10/arch/mips/include/asm/hardirq.h \
  include/asm-generic/hardirq.h \
  include/linux/irq_cpustat.h \
  include/linux/irq.h \
    $(wildcard include/config/generic/pending/irq.h) \
    $(wildcard include/config/hardirqs/sw/resend.h) \
  /home/thunder/new/wusb/openwrt/build_dir/target-mipsel_dsp_uClibc-0.9.33.2/linux-ramips_rt305x/linux-3.10.10/arch/mips/include/asm/irq.h \
    $(wildcard include/config/i8259.h) \
    $(wildcard include/config/mips/mt/smtc/irqaff.h) \
    $(wildcard include/config/mips/mt/smtc/im/backstop.h) \
  include/linux/irqdomain.h \
    $(wildcard include/config/irq/domain.h) \
    $(wildcard include/config/of/irq.h) \
  include/linux/radix-tree.h \
  /home/thunder/new/wusb/openwrt/build_dir/target-mipsel_dsp_uClibc-0.9.33.2/linux-ramips_rt305x/linux-3.10.10/arch/mips/include/asm/mipsmtregs.h \
  /home/thunder/new/wusb/openwrt/build_dir/target-mipsel_dsp_uClibc-0.9.33.2/linux-ramips_rt305x/linux-3.10.10/arch/mips/include/asm/mach-generic/irq.h \
    $(wildcard include/config/irq/cpu/rm7k.h) \
  /home/thunder/new/wusb/openwrt/build_dir/target-mipsel_dsp_uClibc-0.9.33.2/linux-ramips_rt305x/linux-3.10.10/arch/mips/include/asm/irq_regs.h \
  include/linux/irqdesc.h \
    $(wildcard include/config/irq/preflow/fasteoi.h) \
    $(wildcard include/config/sparse/irq.h) \
  /home/thunder/new/wusb/openwrt/build_dir/target-mipsel_dsp_uClibc-0.9.33.2/linux-ramips_rt305x/linux-3.10.10/arch/mips/include/asm/hw_irq.h \
  include/linux/net.h \
  include/linux/random.h \
    $(wildcard include/config/arch/random.h) \
  include/uapi/linux/random.h \
    $(wildcard include/config/fips/rng.h) \
  /home/thunder/new/wusb/openwrt/build_dir/target-mipsel_dsp_uClibc-0.9.33.2/linux-ramips_rt305x/linux-3.10.10/include/uapi/linux/ioctl.h \
  /home/thunder/new/wusb/openwrt/build_dir/target-mipsel_dsp_uClibc-0.9.33.2/linux-ramips_rt305x/linux-3.10.10/arch/mips/include/uapi/asm/ioctl.h \
  include/asm-generic/ioctl.h \
  include/uapi/asm-generic/ioctl.h \
  include/linux/fcntl.h \
  include/uapi/linux/fcntl.h \
  /home/thunder/new/wusb/openwrt/build_dir/target-mipsel_dsp_uClibc-0.9.33.2/linux-ramips_rt305x/linux-3.10.10/arch/mips/include/uapi/asm/fcntl.h \
  /home/thunder/new/wusb/openwrt/build_dir/target-mipsel_dsp_uClibc-0.9.33.2/linux-ramips_rt305x/linux-3.10.10/include/uapi/asm-generic/fcntl.h \
  include/linux/kmemcheck.h \
  include/uapi/linux/net.h \
  include/linux/socket.h \
  /home/thunder/new/wusb/openwrt/build_dir/target-mipsel_dsp_uClibc-0.9.33.2/linux-ramips_rt305x/linux-3.10.10/arch/mips/include/asm/socket.h \
  /home/thunder/new/wusb/openwrt/build_dir/target-mipsel_dsp_uClibc-0.9.33.2/linux-ramips_rt305x/linux-3.10.10/arch/mips/include/uapi/asm/socket.h \
  /home/thunder/new/wusb/openwrt/build_dir/target-mipsel_dsp_uClibc-0.9.33.2/linux-ramips_rt305x/linux-3.10.10/arch/mips/include/uapi/asm/sockios.h \
  /home/thunder/new/wusb/openwrt/build_dir/target-mipsel_dsp_uClibc-0.9.33.2/linux-ramips_rt305x/linux-3.10.10/include/uapi/linux/sockios.h \
  include/linux/uio.h \
  include/uapi/linux/uio.h \
  include/uapi/linux/socket.h \
  include/linux/usb.h \
    $(wildcard include/config/usb/mon.h) \
  include/linux/mod_devicetable.h \
  include/linux/uuid.h \
  include/uapi/linux/uuid.h \
  include/linux/usb/ch9.h \
  include/uapi/linux/usb/ch9.h \
    $(wildcard include/config/size.h) \
    $(wildcard include/config/att/one.h) \
    $(wildcard include/config/att/selfpower.h) \
    $(wildcard include/config/att/wakeup.h) \
    $(wildcard include/config/att/battery.h) \
  include/linux/delay.h \
  /home/thunder/new/wusb/openwrt/build_dir/target-mipsel_dsp_uClibc-0.9.33.2/linux-ramips_rt305x/linux-3.10.10/arch/mips/include/asm/delay.h \
  include/linux/fs.h \
    $(wildcard include/config/fs/posix/acl.h) \
    $(wildcard include/config/quota.h) \
    $(wildcard include/config/fsnotify.h) \
    $(wildcard include/config/ima.h) \
    $(wildcard include/config/debug/writecount.h) \
    $(wildcard include/config/file/locking.h) \
    $(wildcard include/config/fs/xip.h) \
    $(wildcard include/config/direct/io.h) \
    $(wildcard include/config/migration.h) \
  include/linux/kdev_t.h \
  include/uapi/linux/kdev_t.h \
  include/linux/dcache.h \
  include/linux/rculist_bl.h \
  include/linux/list_bl.h \
  include/linux/bit_spinlock.h \
  include/linux/path.h \
  include/linux/semaphore.h \
  /home/thunder/new/wusb/openwrt/build_dir/target-mipsel_dsp_uClibc-0.9.33.2/linux-ramips_rt305x/linux-3.10.10/include/uapi/linux/fiemap.h \
  include/linux/shrinker.h \
  include/linux/migrate_mode.h \
  include/linux/percpu-rwsem.h \
  include/linux/blk_types.h \
    $(wildcard include/config/blk/cgroup.h) \
    $(wildcard include/config/blk/dev/integrity.h) \
  include/uapi/linux/fs.h \
  /home/thunder/new/wusb/openwrt/build_dir/target-mipsel_dsp_uClibc-0.9.33.2/linux-ramips_rt305x/linux-3.10.10/include/uapi/linux/limits.h \
  include/linux/quota.h \
    $(wildcard include/config/quota/netlink/interface.h) \
  /home/thunder/new/wusb/openwrt/build_dir/target-mipsel_dsp_uClibc-0.9.33.2/linux-ramips_rt305x/linux-3.10.10/include/uapi/linux/dqblk_xfs.h \
  include/linux/dqblk_v1.h \
  include/linux/dqblk_v2.h \
  include/linux/dqblk_qtree.h \
  include/linux/projid.h \
  include/uapi/linux/quota.h \
  include/linux/nfs_fs_i.h \
  include/linux/pm_runtime.h \
  drivers/staging/usbip/stub.h \
  include/linux/slab.h \
    $(wildcard include/config/slab/debug.h) \
    $(wildcard include/config/failslab.h) \
    $(wildcard include/config/slob.h) \
    $(wildcard include/config/slab.h) \
    $(wildcard include/config/slub.h) \
    $(wildcard include/config/debug/slab.h) \
  include/linux/slub_def.h \
    $(wildcard include/config/slub/stats.h) \
    $(wildcard include/config/memcg/kmem.h) \
    $(wildcard include/config/slub/debug.h) \
  include/linux/kmemleak.h \
    $(wildcard include/config/debug/kmemleak.h) \

drivers/staging/usbip/stub_dev.o: $(deps_drivers/staging/usbip/stub_dev.o)

$(deps_drivers/staging/usbip/stub_dev.o):
