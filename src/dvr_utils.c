#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <dvr_types.h>
#include <dvr_utils.h>

#ifdef __ANDROID_API__
#include <cutils/properties.h>
#endif

#define _GNU_SOURCE
#define __USE_GNU
#include <search.h>

static struct hsearch_data  *prop_htab = NULL;

/****************************************************************************
 * Macro definitions
 ***************************************************************************/

/****************************************************************************
 * Static functions
 ***************************************************************************/

/****************************************************************************
 * API functions
 ***************************************************************************/


/**\brief Write a string cmd to a file
 * \param[in] name, File name
 * \param[in] cmd, String command
 * \return DVR_SUCCESS On success
 * \return Error code On failure
 */

int dvr_file_echo(const char *name, const char *cmd)
{
  int fd, len, ret;
  if (name == NULL || cmd == NULL) {
    return DVR_FAILURE;
  }

  fd = open(name, O_WRONLY);
  if (fd == -1)
  {
    DVR_INFO("cannot open file \"%s\"", name);
    return DVR_FAILURE;
  }

  len = strlen(cmd);

  ret = write(fd, cmd, len);
  if (ret != len)
  {
    DVR_INFO("write failed file:\"%s\" cmd:\"%s\" error:\"%s\"", name, cmd, strerror(errno));
    close(fd);
    return DVR_FAILURE;
  }

  close(fd);
  return DVR_SUCCESS;
}

/**\brief read sysfs file
 * \param[in] name, File name
 * \param[out] buf, store sysfs node value
 * \return DVR_SUCCESS On success
 * \return Error code On failure
 */

int dvr_file_read(const char *name, char *buf, int len)
{
  FILE *fp;
  char *ret;

  if (name == NULL || buf == NULL) {
    DVR_INFO("dvr_file_read error param is NULL");
    return DVR_FAILURE;
  }

  fp = fopen(name, "r");
  if (!fp)
  {
    DVR_INFO("cannot open file \"%s\"", name);
    return DVR_FAILURE;
  }

  ret = fgets(buf, len, fp);
  if (!ret)
  {
    DVR_INFO("read the file:\"%s\" error:\"%s\" failed", name, strerror(errno));
  }

  fclose(fp);
  return ret ? DVR_SUCCESS : DVR_FAILURE;
}

/**\brief read property value
 * \param[in] name, property name
 * \param[out] buf, property value
 * \return DVR_SUCCESS On success
 * \return DVR_FAILURE On failure
 */
int dvr_prop_read(const char *name, char *buf, int len)
{
  if (name == NULL || buf == NULL) {
    DVR_ERROR("%s, property name or value buffer is NULL",__func__);
    return DVR_FAILURE;
  }

#ifdef __ANDROID_API__
  memset(buf,0,len);
  property_get(name, buf, "");
  if (strlen(buf)>0) {
    DVR_INFO("%s, Read property, name:%s, value:%s",__func__,name,buf);
    return DVR_SUCCESS;
  }
#endif

  if (prop_htab == NULL) {
    prop_htab = calloc(1,sizeof(struct hsearch_data));
    if (prop_htab == NULL) {
      DVR_ERROR("%s, Failed to allocate memory for prop_htab",__func__);
      return DVR_FAILURE;
    }
    if (0 == hcreate_r(100,prop_htab))
    {
      DVR_ERROR("%s, Failed to create hash table with hcreate_r",__func__);
      return DVR_FAILURE;
    }
  }

  ENTRY e = {name,NULL}, *ep = NULL;
  if (hsearch_r(e,FIND,&ep,prop_htab) == 0) {
    DVR_ERROR("%s, Failed to read property %s",__func__,name);
    return DVR_FAILURE;
  }

  strncpy(buf,ep->data,len);
  DVR_INFO("%s, Read property from hash table, name:%s, value:%s",__func__,name,buf);
  return DVR_SUCCESS;
}

/**\brief write property value
 * \param[in] name, property name
 * \param[in] value, property value
 * \return DVR_SUCCESS On success
 * \return DVR_FAILURE On failure
 */
int dvr_prop_write(const char *name, char *value)
{
  if (name == NULL || value == NULL) {
    DVR_ERROR("%s: property name or value buffer is NULL",__func__);
    return DVR_FAILURE;
  }

#ifdef __ANDROID_API__
  property_set(name, value);
#endif

  if (prop_htab == NULL) {
    prop_htab = calloc(1,sizeof(struct hsearch_data));
    if (prop_htab == NULL) {
      DVR_ERROR("%s, Failed to allocate memory for prop_htab",__func__);
      return DVR_FAILURE;
    }
    if (0 == hcreate_r(100,prop_htab))
    {
      DVR_ERROR("%s, Failed to create hash table with hcreate_r",__func__);
      return DVR_FAILURE;
    }
  }

  ENTRY e = {name,NULL}, *ep = NULL;
  if (hsearch_r(e,FIND,&ep,prop_htab) != 0) {
    // in case matched item is found
    free(ep->data);
    ep->data=strdup(value);
  } else {
    // in case no matched item, we need to add new one to hash table
    e.key=strdup(name);
    e.data=strdup(value);
    if ( e.key != NULL && e.data != NULL ) {
      if (hsearch_r(e,ENTER,&ep,prop_htab) == 0) {
        DVR_ERROR("%s, Failed to add an entry to hash table %s:%s",__func__,name,value);
        return DVR_FAILURE;
      }
    } else {
      if (e.key != NULL) {
        free(e.key);
      }
      if (e.data != NULL) {
        free(e.data);
      }
      DVR_ERROR("%s, Failed to duplicate strings %s,%s",__func__,name,value);
      return DVR_FAILURE;
    }
  }

  DVR_INFO("%s, Wrote property to hash table, name:%s, value:%s",__func__,name,value);
  return DVR_SUCCESS;
}

/**\brief read int type property value
 * \param[in] name, property name
 * \param[in] def, default property value in case any failure
 * \return int type property value. If any failure default value will be returned instead
 */
int dvr_prop_read_int(const char *name, int def)
{
  char buf[16] = {0};
  if (dvr_prop_read(name,buf,sizeof(buf)) == DVR_SUCCESS) {
    return atoi(buf);
  }
  return def;
}

#define NSEC_PER_SEC 1000000000L
void clock_timespec_subtract(struct timespec *ts1, struct timespec *ts2, struct timespec *ts3)
{
  time_t sec;
  long nsec;
  sec = ts1->tv_sec - ts2->tv_sec;
  nsec = ts1->tv_nsec - ts2->tv_nsec;
  if (ts1->tv_sec >= 0 && ts1->tv_nsec >=0) {
    if ((sec < 0 && nsec > 0) || (sec > 0 && nsec >= NSEC_PER_SEC)) {
      nsec -= NSEC_PER_SEC;
      sec++;
    }
    if (sec > 0 && nsec < 0) {
      nsec += NSEC_PER_SEC;
      sec--;
    }
  } else {
    if (nsec <= -NSEC_PER_SEC || nsec >= NSEC_PER_SEC) {
      nsec += NSEC_PER_SEC;
      sec--;
    }
    if ((sec < 0 && nsec > 0)) {
      nsec -= NSEC_PER_SEC;
      sec++;
    }
  }
  ts3->tv_sec = sec;
  ts3->tv_nsec = nsec;
}

