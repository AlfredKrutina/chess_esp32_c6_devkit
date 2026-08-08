import Cocoa
import FlutterMacOS

class MainFlutterWindow: NSWindow {
  override func awakeFromNib() {
    let flutterViewController = FlutterViewController()
    self.contentViewController = flutterViewController

    RegisterGeneratedPlugins(registry: flutterViewController)

    let messenger = flutterViewController.engine.binaryMessenger
    CzechMateMacPlatformChannels.registerLiveActivityAndWatch(messenger: messenger)

    // Width for the desktop shell (rail ≥720 + board + panel); default XIB is often 800×600 — too narrow.
    minSize = NSSize(width: 940, height: 640)
    var frame = self.frame
    frame.size = NSSize(width: 1320, height: 860)
    setFrame(frame, display: true)
    center()

    super.awakeFromNib()
  }
}
