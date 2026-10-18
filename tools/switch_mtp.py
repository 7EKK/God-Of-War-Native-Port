"""Files on a Switch running DBI's "Run MTP responder" (USB), through the Windows Portable Devices API.

The Windows shell (Shell.Application CopyHere, what switch_pull_log.ps1 uses) cannot replace or delete files on an
MTP device without a dialog; this talks to the device directly, so files can be listed, read, replaced and deleted.
Needs `pip install comtypes`. Paths are relative to the SD card ("1: SD Card"), with / separators.

usage: python -I tools/switch_mtp.py ls <dir>
       python -I tools/switch_mtp.py get <file> <local file>
       python -I tools/switch_mtp.py put <local file> <dir>     (replaces a file of the same name, creates <dir>)
       python -I tools/switch_mtp.py rm <file>
"""
import ctypes
import os
import sys

import comtypes
import comtypes.client
from comtypes import GUID

comtypes.client.GetModule('portabledeviceapi.dll')
comtypes.client.GetModule('portabledevicetypes.dll')
from comtypes.gen import PortableDeviceApiLib as api  # noqa: E402
from comtypes.gen import PortableDeviceTypesLib as types  # noqa: E402  (coclasses; interfaces and keys from api)

_OBJ = GUID('{EF6B490D-5CD8-437A-AFFC-DA8B60EE4A3C}')  # WPD_OBJECT_PROPERTIES_V1
_RESOURCE_DEFAULT = (GUID('{E81E79BE-34F0-41BF-B53F-F1A06AE87842}'), 0)
_FOLDER = GUID('{27E2E392-A111-48E0-AB0C-E17705A05F85}')
_GENERIC_FILE = GUID('{0085E0A6-8D34-45D7-BC5C-447E59C73D48}')
_FORMAT_UNSPECIFIED = GUID('{30000000-AE6C-4804-98BA-C57B46965FE7}')
PARENT_ID, NAME, CONTENT_TYPE, FORMAT, SIZE, ORIGINAL_FILE_NAME = 3, 4, 7, 6, 11, 12
STGM_READ = 0


def _key(fmtid, pid):
    k = api._tagpropertykey()
    k.fmtid = fmtid
    k.pid = pid
    return ctypes.pointer(k)


def _raw(obj, interface, method):
    # the vtable entry itself: comtypes' wrappers mangle the [in, out] pointer parameters these calls need
    return getattr(obj, f'_{interface}__com_{method}')


def _values():
    return comtypes.client.CreateObject(types.PortableDeviceValues, clsctx=comtypes.CLSCTX_INPROC_SERVER,
                                        interface=api.IPortableDeviceValues)


class Switch:
    def __init__(self):
        manager = comtypes.client.CreateObject(api.PortableDeviceManager, clsctx=comtypes.CLSCTX_INPROC_SERVER,
                                               interface=api.IPortableDeviceManager)
        count = ctypes.c_ulong(0)
        get_devices = _raw(manager, 'IPortableDeviceManager', 'GetDevices')
        get_name = _raw(manager, 'IPortableDeviceManager', 'GetDeviceFriendlyName')
        get_devices(ctypes.POINTER(ctypes.c_wchar_p)(), ctypes.byref(count))
        ids = (ctypes.c_wchar_p * max(count.value, 1))()
        get_devices(ctypes.cast(ids, ctypes.POINTER(ctypes.c_wchar_p)), ctypes.byref(count))
        self.device = None
        for device_id in ids[:count.value]:
            length = ctypes.c_ulong(0)
            get_name(device_id, ctypes.POINTER(ctypes.c_ushort)(), ctypes.byref(length))
            name = ctypes.create_unicode_buffer(length.value)
            get_name(device_id, ctypes.cast(name, ctypes.POINTER(ctypes.c_ushort)), ctypes.byref(length))
            if 'Switch' in name.value or 'DBI' in name.value:
                self.device = comtypes.client.CreateObject(api.PortableDevice, clsctx=comtypes.CLSCTX_INPROC_SERVER,
                                                           interface=api.IPortableDevice)
                self.device.Open(device_id, _values())
                break
        if self.device is None:
            sys.exit('No Switch MTP device: open DBI, choose "Run MTP responder" and connect the USB cable.')
        self.content = self.device.Content()
        self.properties = self.content.Properties()
        self.keys = comtypes.client.CreateObject(types.PortableDeviceKeyCollection,
                                                 clsctx=comtypes.CLSCTX_INPROC_SERVER,
                                                 interface=api.IPortableDeviceKeyCollection)
        for pid in (NAME, ORIGINAL_FILE_NAME, SIZE, CONTENT_TYPE):
            self.keys.Add(_key(_OBJ, pid))
        self.sd = next(o for o in self.children('DEVICE') if 'SD Card' in o['name'] and 'install' not in o['name'])['id']

    def children(self, parent):
        objects = self.content.EnumObjects(0, parent, None)
        result = []
        while True:
            fetched = ctypes.c_ulong(0)
            ids = (ctypes.c_wchar_p * 32)()
            _raw(objects, 'IEnumPortableDeviceObjectIDs', 'Next')(32, ctypes.cast(ids, ctypes.POINTER(ctypes.c_wchar_p)),
                                                                   ctypes.byref(fetched))
            if fetched.value == 0:
                return result
            for object_id in ids[:fetched.value]:
                values = self.properties.GetValues(object_id, self.keys)
                name = ''
                for pid in (ORIGINAL_FILE_NAME, NAME):
                    try:
                        name = values.GetStringValue(_key(_OBJ, pid))
                        break
                    except comtypes.COMError:
                        pass
                try:
                    size = values.GetUnsignedLargeIntegerValue(_key(_OBJ, SIZE))
                except comtypes.COMError:
                    size = None
                try:
                    folder = values.GetGuidValue(_key(_OBJ, CONTENT_TYPE)) in (_FOLDER, GUID(
                        '{99ED0160-17FF-4C44-9D98-1D7A6F941921}'))
                except comtypes.COMError:
                    folder = size is None
                result.append({'id': object_id, 'name': name, 'size': size, 'folder': folder})

    def find(self, path, create=False):
        """object of an SD path ('' = the SD root); with create, missing folders are made"""
        node = {'id': self.sd, 'name': '', 'size': None, 'folder': True}
        for part in [p for p in path.replace('\\', '/').split('/') if p]:
            match = next((c for c in self.children(node['id']) if c['name'] == part), None)
            if match is None:
                if not create:
                    return None
                match = {'id': self._mkdir(node['id'], part), 'name': part, 'size': None, 'folder': True}
            node = match
        return node

    def _mkdir(self, parent, name):
        values = _values()
        values.SetStringValue(_key(_OBJ, PARENT_ID), parent)
        values.SetStringValue(_key(_OBJ, ORIGINAL_FILE_NAME), name)
        values.SetStringValue(_key(_OBJ, NAME), name)
        values.SetGuidValue(_key(_OBJ, CONTENT_TYPE), _FOLDER)
        new_id = ctypes.c_wchar_p()
        _raw(self.content, 'IPortableDeviceContent', 'CreateObjectWithPropertiesOnly')(values, ctypes.byref(new_id))
        return new_id.value

    def delete(self, object_id):
        # a PROPVARIANT holding the id as a string: built by IPortableDeviceValues, which owns that layout
        holder = _values()
        holder.SetStringValue(_key(_OBJ, PARENT_ID), object_id)
        variant = holder.GetValue(_key(_OBJ, PARENT_ID))
        ids = comtypes.client.CreateObject(types.PortableDevicePropVariantCollection,
                                           clsctx=comtypes.CLSCTX_INPROC_SERVER,
                                           interface=api.IPortableDevicePropVariantCollection)
        ids.Add(variant)
        _raw(self.content, 'IPortableDeviceContent', 'Delete')(0, ids, None)

    def get(self, path, local):
        node = self.find(path)
        if node is None or node['folder']:
            sys.exit(f'not a file on the SD card: {path}')
        resources = self.content.Transfer()
        block = ctypes.c_ulong(0)
        key = _key(*_RESOURCE_DEFAULT)
        stream = ctypes.POINTER(api.IStream)()
        _raw(resources, 'IPortableDeviceResources', 'GetStream')(node['id'], key, STGM_READ,
                                                                 ctypes.byref(block), ctypes.byref(stream))
        with open(local, 'wb') as out:
            while True:
                data, read = stream.RemoteRead(max(block.value, 65536))
                if read == 0:
                    break
                out.write(bytes(data[:read]))
        return os.path.getsize(local)

    def put(self, local, directory):
        folder = self.find(directory, create=True)
        name = os.path.basename(local)
        for old in [c for c in self.children(folder['id']) if c['name'] == name]:
            self.delete(old['id'])
        size = os.path.getsize(local)
        values = _values()
        values.SetStringValue(_key(_OBJ, PARENT_ID), folder['id'])
        values.SetUnsignedLargeIntegerValue(_key(_OBJ, SIZE), size)
        values.SetStringValue(_key(_OBJ, ORIGINAL_FILE_NAME), name)
        values.SetStringValue(_key(_OBJ, NAME), name)
        values.SetGuidValue(_key(_OBJ, CONTENT_TYPE), _GENERIC_FILE)
        values.SetGuidValue(_key(_OBJ, FORMAT), _FORMAT_UNSPECIFIED)
        block = ctypes.c_ulong(0)
        cookie = ctypes.c_wchar_p()
        stream = ctypes.POINTER(api.IStream)()
        _raw(self.content, 'IPortableDeviceContent', 'CreateObjectWithPropertiesAndData')(
            values, ctypes.byref(stream), ctypes.byref(block), ctypes.byref(cookie))
        chunk = max(block.value, 1 << 20)
        with open(local, 'rb') as src:
            while True:
                data = src.read(chunk)
                if not data:
                    break
                buffer = (ctypes.c_ubyte * len(data)).from_buffer_copy(data)
                stream.RemoteWrite(buffer, len(data))
        stream.Commit(0)
        written = next((c for c in self.children(folder['id']) if c['name'] == name), None)
        if written is None or written['size'] != size:
            sys.exit(f'upload of {name} incomplete ({written and written["size"]} of {size} bytes)')
        return size


def main(argv):
    if len(argv) < 2:
        sys.exit(__doc__)
    switch = Switch()
    command = argv[1]
    if command == 'ls':
        node = switch.find(argv[2] if len(argv) > 2 else '')
        if node is None:
            sys.exit('no such folder')
        for c in switch.children(node['id']):
            print(f"{'<dir>' if c['folder'] else c['size']:>12}  {c['name']}")
    elif command == 'get':
        print(f'{switch.get(argv[2], argv[3])} bytes -> {argv[3]}')
    elif command == 'put':
        print(f'{switch.put(argv[2], argv[3])} bytes -> {argv[3]}/{os.path.basename(argv[2])}')
    elif command == 'rm':
        node = switch.find(argv[2])
        if node is None:
            sys.exit('no such file')
        switch.delete(node['id'])
        print('deleted', argv[2])
    else:
        sys.exit(__doc__)


if __name__ == '__main__':
    main(sys.argv)
