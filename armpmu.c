/*
 * Enable user-mode ARM performance counter access for Armv6/7/8
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/smp.h>

#define DRVR_NAME "armpmu"

unsigned int mir=0, dfr=0, pnum=0, pmuver=0, pmumaj=0, pmumin=0, dbgmdl=0, plmaj=0, plmin=0;

static void probepmu(void) {
	#if defined(__arm__)
		// below address spaces is common for armv6, armv7, armv8 aarch32
		// main id register
		asm volatile("MRC p15, 0, %0, c0, c0, 0" : "=r"(mir));
		// debug feature register 0
		if ((mir >> 16 & 0xF) > 7){
			// check dfr only > armv6. arm11 is specified as 0xF(15)
			asm volatile("MRC p15, 0, %0, c0, c1, 2" : "=r"(dfr));
		}
		pmuver = dfr >> 24 & 0xF;
		// aarch32, armv7, armv6 pmuver 1 points to ver 1
		if (pmuver == 1){
			pmumaj = 1;
			pmumin = 0;
		}
	#else
		#if defined(__aarch64__)
			asm volatile("MRS %0, MIDR_EL1" : "=r"(mir));
			asm volatile("MRS %0, ID_AA64DFR0_EL1" : "=r"(dfr));
			pmuver = dfr >> 8 & 0xF;
			// aarch64 pmuver 1 points to ver 3
			if (pmuver == 1){
				pmumaj = 3;
				pmumin = 0;
			}
		#else
			#error Module can only be compiled on ARM machines.
		#endif
	#endif
	dbgmdl = dfr & 0xF;
	pnum = mir >> 4 & 0xFFF;
	printk(KERN_DEBUG DRVR_NAME ": dbgmdl:%d, pmuver:%d, pnum:%d", dbgmdl, pmuver, pnum);
	switch (dbgmdl){
		case 2:
			plmaj = 6; plmin = 0; break;
		case 3:
			plmaj = 6; plmin = 1; break;
		case 4:
			plmaj = 7; plmin = 0; break;
		case 5:
			plmaj = 7; plmin = 1; break;
		case 6:
			plmaj = 8; plmin = 0; break;
		case 7:
			plmaj = 8; plmin = 1; break;
		case 8:
			plmaj = 8; plmin = 2; break;
		case 9:
			plmaj = 8; plmin = 4; break;
	}
	switch (pmuver){
		case 0:
			if(plmaj == 6){
				// i am assuming armv6 pmus are are v1.0, it is not defined in the trms.
				pmumaj = 1; pmumin = 0;
			} else if (plmaj == 8){
				// in armv6, v7, pmuver 0 means there may be an unspecific pmu
				// pmuver 15 means there is no pmu implemented, this is vice versa
				// in armv8, we are converting armv8 to old notatiton
				pmumaj = 15;
			}
			break;
		case 2:
			pmumaj = 2; pmumin = 0; break;
		case 3:
			pmumaj = 3; pmumin = 0; break;
		case 4:
			pmumaj = 3; pmumin = 1; break;
		case 5:
			pmumaj = 3; pmumin = 4; break;
		case 6:
			pmumaj = 3; pmumin = 5; break;
		case 15:
			if (plmaj == 0){
				// vice versa in armv8
				pmumaj = 0;
			}
	}
}


static void write_avcr(int val){
	// write to access validation control register
	#if defined(__arm__)
	if(plmaj == 6){
		asm volatile("MCR p15, 0, %0, c15, c9, 0" :: "r"(val));
	} else {
		// common for aarch32 and armv7
		asm volatile("MCR p15, 0, %0, c9, c14, 0" :: "r"(val));
	}
	#else
		#if defined(__aarch64__)
			asm volatile("MSR PMUSERENR_EL0, %0" :: "r"(val));
		#endif
	#endif
}


static void write_pmcr(int val){
	// write to performance monitor control register
	#if defined(__arm__)
	if(plmaj == 6){
		asm volatile("MCR p15, 0, %0, c15, c12, 0" :: "r"(val));
	} else {
		// common for aarch32 and armv7
		asm volatile("mcr p15, 0, %0, c9, c12, 0" :: "r"(val));
	}
	#else
		#if defined(__aarch64__)
			asm volatile("MSR PMUSERENR_EL0, %0" :: "r"(val));
		#endif
	#endif
}


static void set_pmuints(int isset){
	// write to performance monitor interrupt clear/set registers
	#if defined(__arm__)
	switch (plmaj){
		// armv6 does not have this??
		case 7:
			if (isset){
				asm volatile("mcr p15, 0, %0, c9, c12, 1" :: "r"(0xFFFFFFFF));
			} else {
				asm volatile("mcr p15, 0, %0, c9, c12, 2" :: "r"(0xFFFFFFFF));
			}
			break;
		case 8:
			if (isset){
				asm volatile("mcr p15, 0, %0, c9, c14, 1" :: "r"(0xFFFFFFFF));
			} else {
				asm volatile("mcr p15, 0, %0, c9, c14, 2" :: "r"(0xFFFFFFFF));
			}
			break;

	}
	#else
		#if defined(__aarch64__)
			if (isset){
				asm volatile("MSR PMINTENSET_EL1, %0" :: "r"(0xFFFFFFFF));
			} else {
				asm volatile("MSR PMINTENCLR_EL1, %0" :: "r"(0xFFFFFFFF));
			}
		#endif
	#endif
}

static void enable_cpu_counters(void *data) {
	printk(KERN_INFO DRVR_NAME ": enabling user-mode PMU access on CPU #%d", smp_processor_id());
	// enable user space
	write_avcr(1);
	// do not initialize pmcr, this is up to user how to config it
	// enable all pmu interrupts
	set_pmuints(1);
}


static void disable_cpu_counters(void *data) {
	printk(KERN_INFO DRVR_NAME ": disabling user-mode PMU access on CPU #%d", smp_processor_id());
	// disable user space
	write_avcr(0);
	// reset pmcr just in case
	write_pmcr(0);
	// disable pmu interrupts
	set_pmuints(0);
}

static int __init init(void) {
	probepmu();
	if(pmuver == 15){
		printk(KERN_ERR DRVR_NAME ": Can not probe from product code: %d \n", pnum);
	} else {
		printk(KERN_INFO DRVR_NAME ": Probed PMUv%d.%d on ARM platfrom %d.%d for Product Code %d\n", pmumaj, pmumin, plmaj, plmin, pnum);
		on_each_cpu(enable_cpu_counters, NULL, 1);
	}
	return 0;
}

static void __exit fini(void) {
	if(pmuver != 15){
		on_each_cpu(disable_cpu_counters, NULL, 1);
	}
}

MODULE_AUTHOR("boogie <boogiepop@gmx.com>");
MODULE_LICENSE("Dual MIT/GPL");
MODULE_DESCRIPTION("Enables user-mode access to ARM* PMU* counters");
MODULE_VERSION("0:0.1-dev");
module_init( init);
module_exit( fini);
