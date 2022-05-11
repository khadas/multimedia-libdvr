#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/eventfd.h>
#include <fcntl.h>
#include <errno.h>
#include <poll.h>
#include <dlfcn.h>
#include <pthread.h>

#include <dmx.h>
#include <ca.h>
#include <am_ca.h>

#define INF(fmt, ...)   fprintf(stdout, fmt, ##__VA_ARGS__)
#define ERR(fmt, ...)   fprintf(stderr, fmt, ##__VA_ARGS__)

#define UNUSED(x) (void)(x)
#define VALID_PID(pid) ((pid < 0x1fff) && (pid >= 0))
#define DVR_DUMP_MAX_PIDS_CNT   16

static int vpid = 0x1fff;
static int apid = 0x1fff;
static int sub = 0x1fff;
static int duration = 60;
static char rec_filename[512] = {0};
static char *pfilename = "/data/dvr-dump.ts";
static pthread_t dvrDumpThread;
static int dumpRunning = 1;
static int scrambled = 0;
static uint32_t *secdmx_handle = NULL;
static uint32_t key_index, iv_index;

/**\brief DVR stream information*/
typedef struct {
    int             fid; /**< DMX Filter ID*/
    uint16_t        pid; /**< Stream PID*/
    int             started; /**< Filter status */
} DVR_DumpStream_t;

/**\brief DVR dump Context information*/
typedef struct {
    int             fd; /**< DVR device file descriptor*/
    int             evtfd; /**< eventfd for poll's exit*/
    DVR_DumpStream_t    streams[DVR_DUMP_MAX_PIDS_CNT]; /**<DVR stream list*/
    int             dvr_dev_id; /**< DVR device id*/
    int             ts_fd; /*DVR dump file descriptor*/
} DVR_DumpContext_t;

static DVR_DumpContext_t dvr_dump_ctx;

static int dvr_dump_init(int dev);
static int dvr_dump_open(DVR_DumpContext_t **pp_ctx, int device);
static int dvr_dump_close(DVR_DumpContext_t *p_ctx);
static int dvr_dump_add_pid(DVR_DumpContext_t *p_ctx, int pid);
static int dvr_dump_start(DVR_DumpContext_t *p_Ctx);
static int dvr_dump_stop(DVR_DumpContext_t *p_ctx);
static void *dvr_dump_thread(void *arg);

static int dvr_dump_init(int dev)
{
    int i;
    int fd;
    char node[32];
    DVR_DumpContext_t *p_ctx = &dvr_dump_ctx;

    memset(p_ctx, 0xff, sizeof(DVR_DumpContext_t));
    for (i = 0; i < DVR_DUMP_MAX_PIDS_CNT; i++)
    {
        p_ctx->streams[i].pid = 0x1fff;
        p_ctx->streams[i].started = 0;
    }

    memset(node, 0, sizeof(node));
    snprintf(node, sizeof(node), "/dev/dvb0.demux%d", dev);
    fd = open(node, O_WRONLY);
    if (fd == -1)
    {
        ERR("open demux device%d failed:%s\n", dev, strerror(errno));
        return -1;
    }
    if (ioctl(fd, DMX_SET_INPUT, INPUT_DEMOD) == -1)
    {
        ERR("set demux source failed:%s\n", strerror(errno));
        return -1;
    }

    if (ioctl(fd, DMX_SET_HW_SOURCE, FRONTEND_TS2) == -1)
    {
        ERR("set demux tsin failed:%s\n", strerror(errno));
        return -1;
    }

    INF("DUMP init OK\n");

    return 0;
}

static int dvr_dump_open(DVR_DumpContext_t **pp_ctx, int device)
{
    int ret;
    char dev_name[32];
    DVR_DumpContext_t *p_ctx = &dvr_dump_ctx;

    p_ctx->ts_fd = open(pfilename, O_CREAT | O_RDWR | O_TRUNC, 0644);
    if (p_ctx->ts_fd == -1)
    {
        ERR("create dump file failed: %s\n", strerror(errno));
        return -1;
    }

    /*Open dvr device*/
    memset(dev_name, 0, sizeof(dev_name));
    snprintf(dev_name, sizeof(dev_name), "/dev/dvb0.dvr%d", device);
    p_ctx->fd = open(dev_name, O_RDONLY);
    if (p_ctx->fd == -1)
    {
        ERR("cannot open \"%s\" (%s)", dev_name, strerror(errno));
        return -1;
    }
    fcntl(p_ctx->fd, F_SETFL, fcntl(p_ctx->fd, F_GETFL, 0) | O_NONBLOCK, 0);
    p_ctx->evtfd = eventfd(0, 0);

    //set dvbcore ringbuf size
    int buf_size = 188 * 1024;
    ret = ioctl(p_ctx->fd, DMX_SET_BUFFER_SIZE, buf_size);
    if (ret == -1)
    {
        ERR("set dvr ringbuf size failed\"%s\"", strerror(errno));
        return -1;
    }
    p_ctx->dvr_dev_id = device;
    *pp_ctx = p_ctx;

    return 0;
}

static int dvr_dump_close(DVR_DumpContext_t *p_ctx)
{
    close(p_ctx->fd);
    close(p_ctx->evtfd);
    close(p_ctx->ts_fd);

    return 0;
}

static int dvr_dump_add_pid(DVR_DumpContext_t *p_ctx, int pid)
{
    int i;
    int ret;
    int fd;
    char dev_name[32];
    struct dmx_pes_filter_params params;
    int dmx_dev_id = p_ctx->dvr_dev_id;

    for (i = 0; i < DVR_DUMP_MAX_PIDS_CNT; i++)
    {
        if (p_ctx->streams[i].pid == 0x1fff)
        {
            break;
        }
    }

    if (i >= DVR_DUMP_MAX_PIDS_CNT)
    {
        ERR("pid count overflow\n");
        return -1;
    }
    snprintf(dev_name, sizeof(dev_name) , "/dev/dvb0.demux%d", dmx_dev_id);
    fd = open(dev_name, O_RDWR);
    if (fd == -1)
    {
        ERR("cannot open \"%s\" (%s)", dev_name, strerror(errno));
        return -1;
    }

    fcntl(fd, F_SETFL, O_NONBLOCK);
    memset(&params, 0, sizeof(params));
    params.pid = pid;
    params.input = DMX_IN_FRONTEND;
    params.output = DMX_OUT_TS_TAP;
    ret = ioctl(fd, DMX_SET_PES_FILTER, &params);
    if (ret == -1)
    {
        ERR("set pes filter failed \"%s\" (%s)", dev_name, strerror(errno));
        return -1;
    }

    p_ctx->streams[i].pid = pid;
    p_ctx->streams[i].fid = fd;
    p_ctx->streams[i].started = 0;

    return 0;
}

static int dvr_dump_start(DVR_DumpContext_t *p_ctx)
{
    int i;
    int ret;

    for (i = 0; i < DVR_DUMP_MAX_PIDS_CNT; i++)
    {
        if (p_ctx->streams[i].pid != 0x1fff)
        {
            ret = ioctl(p_ctx->streams[i].fid, DMX_START, 0);
            if (ret == -1)
            {
                ERR("start pes filter failed");
                continue;
            }
            p_ctx->streams[i].started = 1;
        }
    }

    return 0;
}

static int dvr_dump_stop(DVR_DumpContext_t *p_ctx)
{
    int i;
    int ret;

    for (i = 0; i < DVR_DUMP_MAX_PIDS_CNT; i++)
    {
        if (p_ctx->streams[i].started)
        {
            ret = ioctl(p_ctx->streams[i].fid, DMX_STOP, 0);
            if (ret == -1)
            {
                ERR("stop pes filter failed (%s)", strerror(errno));
                continue;
            }
            p_ctx->streams[i].pid = 0x1fff;
            close(p_ctx->streams[i].fid);
            p_ctx->streams[i].fid = -1;
            p_ctx->streams[i].started = 0;
        }
    }

    return 0;
}

#define BLOCK_SIZE  (10 * 188 * 1024)
static void *dvr_dump_thread(void *arg)
{
    DVR_DumpContext_t *p_ctx = *(DVR_DumpContext_t **)arg;
    struct pollfd fds[2];
    int timeout = 100;
    int ret;
    int act_len;

    uint8_t *buf = malloc(BLOCK_SIZE);
    int len = BLOCK_SIZE;

    while (dumpRunning) {
        memset(fds, 0, sizeof(fds));

        fds[0].fd = p_ctx->fd;
        fds[1].fd = p_ctx->evtfd;

        fds[0].events = fds[1].events = POLLIN | POLLERR;
        ret = poll(fds, 2, timeout);
        if (ret <= 0)
        {
            ERR("%s poll failed[%d]: %s\n", __func__, ret, strerror(errno));
            continue;
        }

        if (!(fds[0].revents & POLLIN))
            continue;

        act_len = read(fds[0].fd, buf, len);
        if (act_len <= 0)
        {
            ERR("%s read failed: %s\n", __func__, strerror(errno));
            continue;
        }

        //INF("%#x bytes read\n", act_len);

        act_len = write(p_ctx->ts_fd, buf, act_len);
        if (act_len == -1)
        {
            ERR("%s write failed: %s\n", __func__, strerror(errno));
            break;
        }
    }

    return NULL;
}

int (*SECDMX_Init_Ptr)(void);
int (*SECDMX_Deinit_Ptr)(void);
int (*SECDMX_AllocKeytable_Ptr)(uint32_t *key_index, uint32_t *iv_index);
int (*SECDMX_FreeKeytable_Ptr)(uint32_t key_index, uint32_t iv_index);
static int secdmx_alloc_keytable(uint32_t *key_index, uint32_t *iv_index)
{
    secdmx_handle = dlopen("libdmx_client.so", RTLD_NOW);
    if (secdmx_handle == NULL)
    {
        ERR("dlopen failed: %s\n", strerror(errno));
        return -1;
    }
    SECDMX_Init_Ptr = dlsym(secdmx_handle, "SECDMX_Init");
    SECDMX_Deinit_Ptr = dlsym(secdmx_handle, "SECDMX_Deinit");
    SECDMX_AllocKeytable_Ptr = dlsym(secdmx_handle, "SECDMX_AllocKeytable");
    SECDMX_FreeKeytable_Ptr = dlsym(secdmx_handle, "SECDMX_FreeKeytable");

    if (SECDMX_Init_Ptr == NULL ||
      SECDMX_Deinit_Ptr == NULL ||
      SECDMX_AllocKeytable_Ptr == NULL ||
      SECDMX_FreeKeytable_Ptr == NULL)
    {
        ERR("secdmx symbol is null\n");
        return -1;
    }

    if (SECDMX_Init_Ptr()) {
        ERR("secdmx init failed\n");
        return -1;
    }

    INF("secdmx init ok\n");

    return SECDMX_AllocKeytable_Ptr(key_index, iv_index);
}

static int secdmx_free_keytable(void)
{
    SECDMX_FreeKeytable_Ptr(key_index, iv_index);
    dlclose(secdmx_handle);

    return 0;
}

static int dvr_dump_installkey(DVR_DumpContext_t *p_ctx, int *streams, int count)
{
    int i;
    int ret;
    int dsc_chan;
    int scb = 2;
    int dsc_dev = p_ctx->dvr_dev_id;
    int dsc_algo = CA_ALGO_AES_ECB_CLR_END;
    int dsc_type = CA_DSC_TSE_TYPE;

    ret = secdmx_alloc_keytable(&key_index, &iv_index);
    if (ret)
    {
        ERR("alloc keytable failed, ret:%#x\n", ret);
        return -1;
    }

    ca_init();
    ret = ca_open(dsc_dev);
    if (ret)
    {
        ERR("open dsc%d failed\n", dsc_dev);
        return -1;
    }
    for (i = 0; i < count; i++)
    {
        if (!VALID_PID(streams[i]))
            continue;

        dsc_chan = ca_alloc_chan(dsc_dev, streams[i], dsc_algo, dsc_type);
        ca_set_scb(dsc_dev, dsc_chan, scb, 0);
        INF("dsc_chan:%d, key_index:%d, scb:%d\n", dsc_chan, key_index, scb);
        ca_set_key(dsc_dev, dsc_chan, CA_KEY_00_TYPE, key_index);
        //ca_set_key(dsc_dev, dsc_chan, CA_KEY_00_IV_TYPE, iv_index);
    }

    return 0;
}

static int dvr_dump_uninstallkey(DVR_DumpContext_t *p_ctx)
{
    int dsc_dev = p_ctx->dvr_dev_id;

    secdmx_free_keytable();
    ca_close(dsc_dev);

    return 0;
}

static void handle_signal(int signal)
{
    DVR_DumpContext_t *p_ctx = &dvr_dump_ctx;

    UNUSED(signal);
    if (scrambled) {
        dvr_dump_uninstallkey(p_ctx);
    }
    dvr_dump_stop(p_ctx);
    dvr_dump_close(p_ctx);
    INF("DVR DUMP exit\n");
    exit(0);
}

static void init_signal_handler(void)
{
    struct sigaction act;
    act.sa_handler = handle_signal;
    sigaction(SIGINT, &act, NULL);
}

static void usage(int argc, char *argv[])
{
    UNUSED(argc);
    INF("Usage: %s [dev=dev_num] [v=pid] [a=pid] [sub=pid] [dur=s] [cas=1/0] [recfile=x]\n", argv[0]);
}

int main(int argc, char *argv[])
{
    int i;
    char buf[32];
    int running = 1;
    int dvr_dev = 0;
    int pmtpid = 0x1fff;
    DVR_DumpContext_t *p_ctx = NULL;

    init_signal_handler();
    for (i = 1; i < argc; i++) {
        if (!strncmp(argv[i], "v=", 2))
            sscanf(argv[i], "v=%i", &vpid);
        else if (!strncmp(argv[i], "a=", 2))
            sscanf(argv[i], "a=%i", &apid);
        else if (!strncmp(argv[i], "sub=", 4))
            sscanf(argv[i], "sub=%i", &sub);
        else if (!strncmp(argv[i], "pmtpid=", 7))
            sscanf(argv[i], "pmtpid=%i", &pmtpid);
        else if (!strncmp(argv[i], "dur=", 4))
            sscanf(argv[i], "dur=%i", &duration);
        else if (!strncmp(argv[i], "recfile=", 8))
            sscanf(argv[i], "recfile=%s", (char *)(&rec_filename));
        else if (!strncmp(argv[i], "dev=", 4))
            sscanf(argv[i], "dev=%i", &dvr_dev);
        else if (!strncmp(argv[i], "cas=", 4))
            sscanf(argv[i], "cas=%i", &scrambled);
        else {
            usage(argc, argv);
            exit(0);
        }
    }

    if (!VALID_PID(vpid) &&
        !VALID_PID(apid) &&
        !VALID_PID(sub) &&
        !VALID_PID(pmtpid)) {
        usage(argc, argv);
        exit(0);
    }

    INF("DVR DUMP ...\n");

    if (strlen(rec_filename)) {
        pfilename = rec_filename;
    }
    if (dvr_dump_init(dvr_dev)) {
        INF("dvr init failed\n");
        return 0;
    }
    if (dvr_dump_open(&p_ctx, dvr_dev)) {
        INF("dvr open failed\n");
        return 0;
    }

    if (VALID_PID(vpid)) {
        dvr_dump_add_pid(p_ctx, vpid);
    }
    if (VALID_PID(apid)) {
        dvr_dump_add_pid(p_ctx, apid);
    }
    if (VALID_PID(sub)) {
        dvr_dump_add_pid(p_ctx, sub);
    }
    if (VALID_PID(pmtpid)) {
        dvr_dump_add_pid(p_ctx, pmtpid);
    }

    if (scrambled) {
        int streams[8];
        int cnt = 0;

        streams[cnt++] = vpid;
        streams[cnt++] = apid;
        dvr_dump_installkey(p_ctx, &streams[0], cnt);
    }

    pthread_create(&dvrDumpThread, NULL, dvr_dump_thread, &p_ctx);
    dvr_dump_start(p_ctx);

    INF("===============\n");
    INF("*quit\n");
    INF("===============\n");
    while (running) {
        memset(buf, 0, sizeof(buf));
        if (fgets(buf, sizeof(buf), stdin)) {
            if (!strncmp(buf, "quit", 4)) {
                running = 0;
                dumpRunning = 0;
                continue;
            }

        }
    }

    if (scrambled) {
        dvr_dump_uninstallkey(p_ctx);
    }
    pthread_join(dvrDumpThread, NULL);
    dvr_dump_stop(p_ctx);
    dvr_dump_close(p_ctx);
    INF("DVR DUMP exit\n");
    exit(0);
}
