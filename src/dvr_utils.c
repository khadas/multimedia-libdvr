#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <dvr_types.h>
#include <dvr_utils.h>
#include <cutils/properties.h>

/****************************************************************************
 * Macro definitions
 ***************************************************************************/

/****************************************************************************
 * Static functions
 ***************************************************************************/
int (*Write_Sysfs_ptr)(const char *path, char *value);
int (*ReadNum_Sysfs_ptr)(const char *path, char *value, int size);
int (*Read_Sysfs_ptr)(const char *path, char *value);

typedef struct dvr_rw_sysfs_cb_s
{
  DVR_Read_Sysfs_Cb readSysfsCb;
  DVR_Write_Sysfs_Cb writeSysfsCb;
}dvr_rw_sysfs_cb_t;

dvr_rw_sysfs_cb_t rwSysfsCb = {.readSysfsCb = NULL, .writeSysfsCb = NULL};

typedef struct dvr_rw_prop_cb_s
{
  DVR_Read_Prop_Cb readPropCb;
  DVR_Write_Prop_Cb writePropCb;
}dvr_rw_prop_cb_t;

dvr_rw_prop_cb_t rwPropCb = {.readPropCb = NULL, .writePropCb = NULL};


/****************************************************************************
 * API functions
 ***************************************************************************/

/**\brief regist rw sysfs cb
 * \param[in] fun callback
 * \return
 *   - DVR_SUCCESS
 *   - error
 */
int dvr_register_rw_sys(DVR_Read_Sysfs_Cb RCb, DVR_Write_Sysfs_Cb WCb)
{

  if (RCb == NULL || WCb == NULL) {
    DVR_DEBUG(1, "dvr_register_rw_sys error param is NULL !!");
    return DVR_FAILURE;
  }
  if (!rwSysfsCb.readSysfsCb)
    rwSysfsCb.readSysfsCb = RCb;
  if (!rwSysfsCb.writeSysfsCb)
    rwSysfsCb.writeSysfsCb = WCb;
  DVR_DEBUG(1, "dvr_register_rw_sys success !!");
  return DVR_SUCCESS;
}

/**\brief unregist rw sys cb
 */
int dvr_unregister_rw_sys()
{
  if (rwSysfsCb.readSysfsCb)
    rwSysfsCb.readSysfsCb = NULL;
  if (rwSysfsCb.writeSysfsCb)
    rwSysfsCb.writeSysfsCb = NULL;
  return DVR_SUCCESS;
}

/**\brief regist rw prop cb
 * \param[in] fun callback
 * \return
 *   - DVR_SUCCESS
 *   - error
 */

int dvr_rgister_rw_prop(DVR_Read_Prop_Cb RCb, DVR_Write_Prop_Cb WCb)
{
  if (RCb == NULL || WCb == NULL) {
    DVR_DEBUG(1, "dvr_rgister_rw_prop error param is NULL !!");
    return DVR_FAILURE;
  }

  if (!rwPropCb.readPropCb)
    rwPropCb.readPropCb = RCb;
  if (!rwPropCb.writePropCb)
    rwPropCb.writePropCb = WCb;

  DVR_DEBUG(1, "dvr_rgister_rw_prop !!");
  return DVR_SUCCESS;
}

/**\brief unregist rw prop cb */
int dvr_unregister_rw_prop()
{
  if (rwPropCb.readPropCb)
    rwPropCb.readPropCb = NULL;
  if (rwPropCb.writePropCb)
    rwPropCb.writePropCb = NULL;
  return DVR_SUCCESS;
}

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

  if (rwSysfsCb.writeSysfsCb)
  {
    rwSysfsCb.writeSysfsCb(name, cmd);
    return DVR_SUCCESS;
  }

  fd = open(name, O_WRONLY);
  if (fd == -1)
  {
    DVR_DEBUG(1, "cannot open file \"%s\"", name);
    return DVR_FAILURE;
  }

  len = strlen(cmd);

  ret = write(fd, cmd, len);
  if (ret != len)
  {
    DVR_DEBUG(1, "write failed file:\"%s\" cmd:\"%s\" error:\"%s\"", name, cmd, strerror(errno));
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
    DVR_DEBUG(1, "dvr_file_read error param is NULL");
    return DVR_FAILURE;
  }

  if (rwSysfsCb.readSysfsCb)
  {
    rwSysfsCb.readSysfsCb(name, buf, len);
    return DVR_SUCCESS;
  }


  fp = fopen(name, "r");
  if (!fp)
  {
    DVR_DEBUG(1, "cannot open file \"%s\"", name);
    return DVR_FAILURE;
  }

  ret = fgets(buf, len, fp);
  if (!ret)
  {
    DVR_DEBUG(1, "read the file:\"%s\" error:\"%s\" failed", name, strerror(errno));
  }

  fclose(fp);
  return ret ? DVR_SUCCESS : DVR_FAILURE;
}


/**\brief Write a string cmd to a prop
 * \param[in] name, prop name
 * \param[in] cmd, String command
 * \return DVR_SUCCESS On success
 * \return Error code On failure
 */

int dvr_prop_echo(const char *name, const char *cmd)
{
  if (name == NULL || cmd == NULL) {
    DVR_DEBUG(1, "dvr_prop_echo: error param is NULL");
    return DVR_FAILURE;
  }

  if (rwPropCb.writePropCb)
  {
    rwPropCb.writePropCb(name, cmd);
    return DVR_SUCCESS;
  }
  property_set(name, cmd);
  DVR_DEBUG(1, "dvr_prop_echo: error writePropCb is NULL, used property_set");
  return DVR_FAILURE;
}

/**\brief read prop value
 * \param[in] name, prop name
 * \param[out] buf, store prop node value
 * \return DVR_SUCCESS On success
 * \return Error code On failure
 */

int dvr_prop_read(const char *name, char *buf, int len)
{
  if (name == NULL || buf == NULL) {
    DVR_DEBUG(1, "dvr_prop_read: error param is NULL");
    return DVR_FAILURE;
  }

  if (rwPropCb.readPropCb)
  {
    rwPropCb.readPropCb(name, buf, len);
    return DVR_SUCCESS;
  }
  property_get(name, buf, "");
  DVR_DEBUG(1, "dvr_prop_read: error readPropCb is NULL, used property_get[%s][%d]", name ,buf);
  return DVR_FAILURE;
}

