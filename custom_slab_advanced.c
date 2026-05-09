/*
 * custom_slab_advanced.c
 *
 * Advanced Linux Kernel SLAB Allocator Demo
 *
 * Features:
 *  - Custom SLAB cache
 *  - Constructor
 *  - Shrinker API
 *  - Refcounting
 *  - Per-CPU statistics
 *  - Procfs runtime stats
 *  - Aging-based reclaim
 *  - Debug object tracking
 *  - Spinlock synchronization
 *  - SLAB poisoning + redzones
 *
 * Build:
 *   make
 *
 * Run:
 *   sudo insmod custom_slab_advanced.ko
 *   dmesg -w
 *
 * Check:
 *   cat /proc/custom_slab_stats
 *   cat /proc/slabinfo | grep my_object_cache
 *
 * Remove:
 *   sudo rmmod custom_slab_advanced
 */

/*
 * custom_slab_advanced.c
 *
 * Compatible Advanced SLAB Allocator Demo
 * (Fixed for older kernel shrinker API)
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/shrinker.h>
#include <linux/refcount.h>
#include <linux/jiffies.h>
#include <linux/percpu.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/version.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kernel Developer");
MODULE_DESCRIPTION("Advanced Custom SLAB Allocator");
MODULE_VERSION("2.1");

/* =========================================================
 * Config
 * ========================================================= */

#define OBJECT_NAME_LEN    32
#define OBJECT_MAX_AGE_MS  10000

/* =========================================================
 * Object Definition
 * ========================================================= */

struct my_object {
    int id;

    char name[OBJECT_NAME_LEN];

    refcount_t refcnt;

    unsigned long created_jiffies;

    struct list_head list;
};

/* =========================================================
 * Per CPU Statistics
 * ========================================================= */

struct slab_stats {
    unsigned long allocs;
    unsigned long frees;
};

static DEFINE_PER_CPU(struct slab_stats, cpu_stats);

/* =========================================================
 * Globals
 * ========================================================= */

static struct kmem_cache *my_cache;

static LIST_HEAD(object_list);

static DEFINE_SPINLOCK(object_lock);

static unsigned long active_objects;

/* =========================================================
 * Constructor
 * ========================================================= */

static void my_obj_ctor(void *obj)
{
    struct my_object *o = obj;

    o->id = -1;

    strscpy(o->name,
            "initialized",
            sizeof(o->name));

    refcount_set(&o->refcnt, 1);

    o->created_jiffies = jiffies;

    INIT_LIST_HEAD(&o->list);
}

/* =========================================================
 * Print Object
 * ========================================================= */

static void my_object_print(struct my_object *obj)
{
    if (!obj)
        return;

    pr_info("[SLAB] id=%d name=%s refcnt=%d age=%u ms\n",
            obj->id,
            obj->name,
            refcount_read(&obj->refcnt),
            jiffies_to_msecs(jiffies -
                             obj->created_jiffies));
}

/* =========================================================
 * Allocate Object
 * ========================================================= */

static struct my_object *
allocate_object(int id, const char *name)
{
    struct my_object *obj;

    obj = kmem_cache_alloc(my_cache, GFP_KERNEL);

    if (!obj) {
        pr_err("[SLAB] allocation failed\n");
        return NULL;
    }

    obj->id = id;

    strscpy(obj->name,
            name,
            sizeof(obj->name));

    refcount_set(&obj->refcnt, 1);

    obj->created_jiffies = jiffies;

    spin_lock(&object_lock);

    list_add_tail(&obj->list,
                  &object_list);

    active_objects++;

    spin_unlock(&object_lock);

    this_cpu_inc(cpu_stats.allocs);

    pr_info("[SLAB] allocated object id=%d\n",
            id);

    return obj;
}

/* =========================================================
 * Free Object
 * ========================================================= */

static void free_object(struct my_object *obj)
{
    if (!obj)
        return;

    spin_lock(&object_lock);

    list_del(&obj->list);

    active_objects--;

    spin_unlock(&object_lock);

    memset(obj, 0xDE, sizeof(*obj));

    kmem_cache_free(my_cache, obj);

    this_cpu_inc(cpu_stats.frees);

    pr_info("[SLAB] object freed\n");
}

/* =========================================================
 * Refcount Helpers
 * ========================================================= */

static void get_object(struct my_object *obj)
{
    if (!obj)
        return;

    refcount_inc(&obj->refcnt);
}

static void put_object(struct my_object *obj)
{
    if (!obj)
        return;

    if (refcount_dec_and_test(&obj->refcnt))
        free_object(obj);
}

/* =========================================================
 * Shrinker Count
 * ========================================================= */

static unsigned long
my_count_objects(struct shrinker *s,
                 struct shrink_control *sc)
{
    return active_objects;
}

/* =========================================================
 * Shrinker Scan
 * ========================================================= */

static unsigned long
my_scan_objects(struct shrinker *s,
                struct shrink_control *sc)
{
    struct my_object *obj;
    struct my_object *tmp;

    unsigned long freed = 0;

    unsigned long age_limit;

    age_limit =
        msecs_to_jiffies(OBJECT_MAX_AGE_MS);

    spin_lock(&object_lock);

    list_for_each_entry_safe(obj,
                             tmp,
                             &object_list,
                             list) {

        if (time_before(jiffies,
                        obj->created_jiffies +
                        age_limit))
            continue;

        list_del(&obj->list);

        active_objects--;

        spin_unlock(&object_lock);

        kmem_cache_free(my_cache, obj);

        this_cpu_inc(cpu_stats.frees);

        freed++;

        pr_info("[SLAB] shrinker reclaimed object\n");

        if (freed >= sc->nr_to_scan)
            return freed;

        spin_lock(&object_lock);
    }

    spin_unlock(&object_lock);

    return freed;
}

/* =========================================================
 * Shrinker Structure
 * ========================================================= */

static struct shrinker my_shrinker = {
    .count_objects = my_count_objects,
    .scan_objects  = my_scan_objects,
    .seeks         = DEFAULT_SEEKS,
};

/* =========================================================
 * Procfs Stats
 * ========================================================= */

static int slab_stats_show(struct seq_file *m,
                           void *v)
{
    int cpu;

    unsigned long total_allocs = 0;
    unsigned long total_frees  = 0;

    struct slab_stats *stats;

    seq_printf(m,
               "==== Custom SLAB Stats ====\n");

    seq_printf(m,
               "Active Objects : %lu\n",
               active_objects);

    for_each_possible_cpu(cpu) {

        stats = &per_cpu(cpu_stats, cpu);

        total_allocs += stats->allocs;

        total_frees += stats->frees;
    }

    seq_printf(m,
               "Total Allocs   : %lu\n",
               total_allocs);

    seq_printf(m,
               "Total Frees    : %lu\n",
               total_frees);

    seq_printf(m,
               "Cache Name     : my_object_cache\n");

    return 0;
}

static int slab_stats_open(struct inode *inode,
                           struct file *file)
{
    return single_open(file,
                       slab_stats_show,
                       NULL);
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,6,0)

static const struct proc_ops proc_fops = {
    .proc_open    = slab_stats_open,
    .proc_read    = seq_read,
    .proc_lseek   = seq_lseek,
    .proc_release = single_release,
};

#else

static const struct file_operations proc_fops = {
    .open    = slab_stats_open,
    .read    = seq_read,
    .llseek  = seq_lseek,
    .release = single_release,
};

#endif

/* =========================================================
 * Init
 * ========================================================= */

static int __init custom_slab_init(void)
{
    struct my_object *obj1;
    struct my_object *obj2;
    struct my_object *obj3;

    pr_info("====================================\n");
    pr_info(" Loading Advanced Custom SLAB Module\n");
    pr_info("====================================\n");

    my_cache = kmem_cache_create(
        "my_object_cache",
        sizeof(struct my_object),
        0,
        SLAB_POISON |
        SLAB_RED_ZONE |
        SLAB_HWCACHE_ALIGN,
        my_obj_ctor
    );

    if (!my_cache) {
        pr_err("[SLAB] cache creation failed\n");
        return -ENOMEM;
    }

    /*
     * OLD KERNEL COMPATIBLE API
     */

    if (register_shrinker(&my_shrinker)) {

        pr_err("[SLAB] shrinker registration failed\n");

        kmem_cache_destroy(my_cache);

        return -EINVAL;
    }

    if (!proc_create("custom_slab_stats",
                     0444,
                     NULL,
                     &proc_fops)) {

        pr_err("[SLAB] procfs creation failed\n");

        unregister_shrinker(&my_shrinker);

        kmem_cache_destroy(my_cache);

        return -ENOMEM;
    }

    obj1 = allocate_object(1,
                           "Object-One");

    obj2 = allocate_object(2,
                           "Object-Two");

    obj3 = allocate_object(3,
                           "Object-Three");

    my_object_print(obj1);
    my_object_print(obj2);
    my_object_print(obj3);

    get_object(obj1);

    put_object(obj1);

    pr_info("[SLAB] module loaded successfully\n");

    return 0;
}

/* =========================================================
 * Exit
 * ========================================================= */

static void __exit custom_slab_exit(void)
{
    struct my_object *obj;
    struct my_object *tmp;

    pr_info("====================================\n");
    pr_info(" Unloading Advanced Custom SLAB\n");
    pr_info("====================================\n");

    remove_proc_entry("custom_slab_stats",
                      NULL);

    unregister_shrinker(&my_shrinker);

    spin_lock(&object_lock);

    list_for_each_entry_safe(obj,
                             tmp,
                             &object_list,
                             list) {

        list_del(&obj->list);

        active_objects--;

        spin_unlock(&object_lock);

        kmem_cache_free(my_cache, obj);

        this_cpu_inc(cpu_stats.frees);

        spin_lock(&object_lock);
    }

    spin_unlock(&object_lock);

    kmem_cache_destroy(my_cache);

    pr_info("[SLAB] module unloaded successfully\n");
}

/* =========================================================
 * Module Hooks
 * ========================================================= */

module_init(custom_slab_init);
module_exit(custom_slab_exit);
