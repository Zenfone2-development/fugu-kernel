/*************************************************************************/ /*!
@File
@Title          Linux module setup
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@License        Dual MIT/GPLv2

The contents of this file are subject to the MIT license as set out below.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

Alternatively, the contents of this file may be used under the terms of
the GNU General Public License Version 2 ("GPL") in which case the provisions
of GPL are applicable instead of those above.

If you wish to allow use of your version of this file only under the terms of
GPL, and not to allow others to use your version of this file under the terms
of the MIT license, indicate your decision by deleting the provisions above
and replace them with the notice and other provisions required by GPL as set
out in the file called "GPL-COPYING" included in this distribution. If you do
not delete the provisions above, a recipient may use your version of this file
under the terms of either the MIT license or GPL.

This License is also included in this distribution in the file called
"MIT-COPYING".

EXCEPT AS OTHERWISE STATED IN A NEGOTIATED AGREEMENT: (A) THE SOFTWARE IS
PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
PURPOSE AND NONINFRINGEMENT; AND (B) IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/ /**************************************************************************/

#include <linux/version.h>
#include <linux/module.h>
#include <linux/device.h>
#include <drm/drmP.h>
#include "pvr_debug.h"
#include "srvkm.h"
#include "pvrmodule.h"
#include "linkage.h"
#include "sysinfo.h"
#include "module_common.h"
#include "syscommon.h"

#if defined(SUPPORT_DRM_EXT)
#include "pvr_drm_ext.h"
#endif

#include "pvr_drm.h"

#if defined(SUPPORT_SHARED_SLC)
#include "rgxapi_km.h"
#endif

/*
 * DRVNAME is the name we use to register our driver.
 * DEVNAME is the name we use to register actual device nodes.
 */
#define	DRVNAME		PVR_LDM_DRIVER_REGISTRATION_NAME
#define DEVNAME		PVRSRV_MODNAME

/*
 * This is all module configuration stuff required by the linux kernel.
 */
MODULE_SUPPORTED_DEVICE(DEVNAME);

#if defined(SUPPORT_SHARED_SLC)
EXPORT_SYMBOL(RGXInitSLC);
#endif

#define	LDM_DRV	struct pci_driver

static void PVRSRVDriverRemove(struct pci_dev *device);
static int PVRSRVDriverProbe(struct pci_dev *device, const struct pci_device_id *id);

/* This structure is used by the Linux module code */
struct pci_device_id powervr_id_table[] __devinitdata = {
	{PCI_DEVICE(SYS_RGX_DEV_VENDOR_ID, SYS_RGX_DEV_DEVICE_ID)},
#if defined (SYS_RGX_DEV1_DEVICE_ID)
	{PCI_DEVICE(SYS_RGX_DEV_VENDOR_ID, SYS_RGX_DEV1_DEVICE_ID)},
#endif
	{0}
};
#if !defined(SUPPORT_DRM_EXT)
MODULE_DEVICE_TABLE(pci, powervr_id_table);
#endif

static struct dev_pm_ops powervr_dev_pm_ops = {
	.suspend	= PVRSRVDriverSuspend,
	.resume		= PVRSRVDriverResume,
};

static LDM_DRV powervr_driver = {
	.name		= DRVNAME,
	.driver.pm	= &powervr_dev_pm_ops,
	.id_table	= powervr_id_table,
	.probe		= PVRSRVDriverProbe,
	.remove		= __devexit_p(PVRSRVDriverRemove),
	.shutdown	= PVRSRVDriverShutdown,
};

static IMG_BOOL bCalledSysInit = IMG_FALSE;
static IMG_BOOL	bDriverProbeSucceeded = IMG_FALSE;

/*!
******************************************************************************

 @Function		PVRSRVSystemInit

 @Description

 Wrapper for PVRSRVInit.

 @input pDevice - the device for which a probe is requested

 @Return 0 for success or <0 for an error.

*****************************************************************************/
int PVRSRVSystemInit(struct drm_device *pDrmDevice)
{
	struct pci_dev *pDevice = pDrmDevice->pdev;

	PVR_TRACE(("PVRSRVSystemInit (pDevice=%p)", pDevice));

	/* PVRSRVInit is only designed to be called once */
	if (bCalledSysInit == IMG_FALSE)
	{
		gpsPVRLDMDev = pDevice;
		bCalledSysInit = IMG_TRUE;

		if (PVRSRVInit(pDevice) != PVRSRV_OK)
		{
			return -ENODEV;
		}
	}

	return 0;
}

/*!
******************************************************************************

 @Function		PVRSRVSystemDeInit

 @Description

 Wrapper for PVRSRVDeInit.

 @input pDevice - the device for which driver detachment is happening
 @Return nothing.

*****************************************************************************/
void PVRSRVSystemDeInit(struct pci_dev *pDevice)
{
	PVR_TRACE(("PVRSRVSystemDeInit"));

	PVRSRVDeInit(pDevice);

	gpsPVRLDMDev = NULL;
}

/*!
******************************************************************************

 @Function		PVRSRVDriverProbe

 @Description

 See whether a given device is really one we can drive.

 @input pDevice - the device for which a probe is requested

 @Return 0 for success or <0 for an error.

*****************************************************************************/
static int __devinit PVRSRVDriverProbe(struct pci_dev *pDevice, const struct pci_device_id *pID)
{
	int result = 0;

	PVR_TRACE(("PVRSRVDriverProbe (pDevice=%p)", pDevice));

#if !defined(SUPPORT_DRM_EXT)
	result = drm_get_pci_dev(pDevice, pID, &sPVRDRMDriver);
#endif

	bDriverProbeSucceeded = (result == 0);
	return result;
}


/*!
******************************************************************************

 @Function		PVRSRVDriverRemove

 @Description

 This call is the opposite of the probe call; it is called when the device is
 being removed from the driver's control.

 @input pDevice - the device for which driver detachment is happening

 @Return 0, or no return value at all, depending on the device type.

*****************************************************************************/
static void __devexit PVRSRVDriverRemove(struct pci_dev *pDevice)
{
	PVR_TRACE(("PVRSRVDriverRemove (pDevice=%p)", pDevice));

#if !defined(SUPPORT_DRM_EXT)
	drm_put_dev(pci_get_drvdata(pDevice));
#else	/* !defined(SUPPORT_DRM_EXT) */
	PVRSRVSystemDeInit(pDevice);
#endif	/* !defined(SUPPORT_DRM_EXT) */
}

/*!
******************************************************************************

 @Function		PVRSRVOpen

 @Description

 Open the PVR services node.

 @input pInode - the inode for the file being openeded.
 @input dev    - the DRM device corresponding to this driver.

 @input pFile - the file handle data for the actual file being opened

 @Return 0 for success or <0 for an error.

*****************************************************************************/
int PVRSRVOpen(struct drm_device unref__ *dev, struct drm_file *pDRMFile)
{
	int err;

	struct file *pFile = PVR_FILE_FROM_DRM_FILE(pDRMFile);

	if (!try_module_get(THIS_MODULE))
	{
		PVR_DPF((PVR_DBG_ERROR, "Failed to get module"));
		return -ENOENT;
	}

	if ((err = PVRSRVCommonOpen(pFile)) != 0)
	{
		module_put(THIS_MODULE);
	}

	return err;
}

/*!
******************************************************************************

 @Function		PVRSRVRelease

 @Description

 Release access the PVR services node - called when a file is closed, whether
 at exit or using close(2) system call.

 @input pInode - the inode for the file being released
 @input pvPrivData - driver private data

 @input pFile - the file handle data for the actual file being released

 @Return 0 for success or <0 for an error.

*****************************************************************************/
void PVRSRVRelease(struct drm_device unref__ *dev, struct drm_file *pDRMFile)
{
	struct file *pFile = PVR_FILE_FROM_DRM_FILE(pDRMFile);

	PVRSRVCommonRelease(pFile);

	module_put(THIS_MODULE);
}

/*!
******************************************************************************

 @Function		PVRCore_Init

 @Description

 Insert the driver into the kernel.

 Readable and/or writable debugfs entries under /sys/kernel/debug/pvr are
 created with PVRDebugFSCreateEntry().  These can be read at runtime to get
 information about the device (eg. 'cat /sys/kernel/debug/pvr/nodes')

 __init places the function in a special memory section that the kernel frees
 once the function has been run.  Refer also to module_init() macro call below.

 @input none

 @Return none

*****************************************************************************/
#if defined(SUPPORT_DRM_EXT)
int PVRCore_Init(void)
#else
static int __init PVRCore_Init(void)
#endif
{
	int error = 0;

	PVR_TRACE(("PVRCore_Init"));

#if defined(PDUMP)
	error = dbgdrv_init();
	if (error != 0)
	{
		return error;
	}
#endif

	if ((error = PVRSRVDriverInit()) != 0)
	{
		return error;
	}

#if !defined(SUPPORT_DRM_EXT)
	error = drm_pci_init(&sPVRDRMDriver, &powervr_driver);
	if (error != 0)
	{
		PVR_DPF((PVR_DBG_ERROR, "PVRCore_Init: unable to register PCI driver (%d)", error));
		return error;
	}
#else
	if (!bDriverProbeSucceeded)
	{
		error = PVRSRVInit(gpsPVRLDMDev);
		if (error != 0)
		{
			PVR_DPF((PVR_DBG_ERROR, "PVRSRVSystemInit: unable to init PVR service (%d)", error));
			return error;
		}
		bDriverProbeSucceeded = IMG_TRUE;
	}
#endif /* defined(SUPPORT_DRM_EXT) */

	if (!bDriverProbeSucceeded)
	{
		PVR_TRACE(("PVRCore_Init: PVRSRVDriverProbe has not been called or did not succeed - check that hardware is detected"));
		return error;
	}

	return PVRSRVDeviceInit();
}


/*!
*****************************************************************************

 @Function		PVRCore_Cleanup

 @Description	

 Remove the driver from the kernel.

 There's no way we can get out of being unloaded other than panicking; we
 just do everything and plough on regardless of error.

 __exit places the function in a special memory section that the kernel frees
 once the function has been run.  Refer also to module_exit() macro call below.

 @input none

 @Return none

*****************************************************************************/
#if defined(SUPPORT_DRM_EXT)
void PVRCore_Cleanup(void)
#else
static void __exit PVRCore_Cleanup(void)
#endif
{
	PVR_TRACE(("PVRCore_Cleanup"));

	PVRSRVDeviceDeinit();
	
#if !defined(SUPPORT_DRM_EXT)
	drm_pci_exit(&sPVRDRMDriver, &powervr_driver);
#else
	pci_unregister_driver(&powervr_driver);
#endif

	PVRSRVDriverDeinit();

#if defined(PDUMP)
	dbgdrv_cleanup();
#endif
	PVR_TRACE(("PVRCore_Cleanup: unloading"));
}

/*
 * These macro calls define the initialisation and removal functions of the
 * driver.  Although they are prefixed `module_', they apply when compiling
 * statically as well; in both cases they define the function the kernel will
 * run to start/stop the driver.
*/
#if !defined(SUPPORT_DRM_EXT)
module_init(PVRCore_Init);
module_exit(PVRCore_Cleanup);
#endif
