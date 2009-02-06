// This file was generated by the Gtk# code generator.
// Any changes made will be lost if regenerated.

namespace PackageKitSharp {

	using System;
	using System.Runtime.InteropServices;

#region Autogenerated code
	[GLib.CDeclCallback]
	internal delegate IntPtr ObjListFromStringFuncNative(IntPtr data);

	internal class ObjListFromStringFuncInvoker {

		ObjListFromStringFuncNative native_cb;
		IntPtr __data;
		GLib.DestroyNotify __notify;

		~ObjListFromStringFuncInvoker ()
		{
			if (__notify == null)
				return;
			__notify (__data);
		}

		internal ObjListFromStringFuncInvoker (ObjListFromStringFuncNative native_cb) : this (native_cb, IntPtr.Zero, null) {}

		internal ObjListFromStringFuncInvoker (ObjListFromStringFuncNative native_cb, IntPtr data) : this (native_cb, data, null) {}

		internal ObjListFromStringFuncInvoker (ObjListFromStringFuncNative native_cb, IntPtr data, GLib.DestroyNotify notify)
		{
			this.native_cb = native_cb;
			__data = data;
			__notify = notify;
		}

		internal PackageKit.ObjListFromStringFunc Handler {
			get {
				return new PackageKit.ObjListFromStringFunc(InvokeNative);
			}
		}

		IntPtr InvokeNative (string data)
		{
			IntPtr native_data = GLib.Marshaller.StringToPtrGStrdup (data);
			IntPtr result = native_cb (native_data);
			GLib.Marshaller.Free (native_data);
			return result;
		}
	}

	internal class ObjListFromStringFuncWrapper {

		public IntPtr NativeCallback (IntPtr data)
		{
			try {
				IntPtr __ret = managed (GLib.Marshaller.Utf8PtrToString (data));
				if (release_on_call)
					gch.Free ();
				return __ret;
			} catch (Exception e) {
				GLib.ExceptionManager.RaiseUnhandledException (e, true);
				// NOTREACHED: Above call does not return.
				throw e;
			}
		}

		bool release_on_call = false;
		GCHandle gch;

		public void PersistUntilCalled ()
		{
			release_on_call = true;
			gch = GCHandle.Alloc (this);
		}

		internal ObjListFromStringFuncNative NativeDelegate;
		PackageKit.ObjListFromStringFunc managed;

		public ObjListFromStringFuncWrapper (PackageKit.ObjListFromStringFunc managed)
		{
			this.managed = managed;
			if (managed != null)
				NativeDelegate = new ObjListFromStringFuncNative (NativeCallback);
		}

		public static PackageKit.ObjListFromStringFunc GetManagedDelegate (ObjListFromStringFuncNative native)
		{
			if (native == null)
				return null;
			ObjListFromStringFuncWrapper wrapper = (ObjListFromStringFuncWrapper) native.Target;
			if (wrapper == null)
				return null;
			return wrapper.managed;
		}
	}
#endregion
}