// This file was generated by the Gtk# code generator.
// Any changes made will be lost if regenerated.

namespace PackageKit {

	using System;

	public delegate void CallerActiveChangedHandler(object o, CallerActiveChangedArgs args);

	public class CallerActiveChangedArgs : GLib.SignalArgs {
		public bool IsActive{
			get {
				return (bool) Args[0];
			}
		}

	}
}