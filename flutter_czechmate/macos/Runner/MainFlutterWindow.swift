import Cocoa
import FlutterMacOS

class MainFlutterWindow: NSWindow {
  override func awakeFromNib() {
    let flutterViewController = FlutterViewController()
    self.contentViewController = flutterViewController

    RegisterGeneratedPlugins(registry: flutterViewController)

    let messenger = flutterViewController.engine.binaryMessenger
    CzechMateMacPlatformChannels.registerLiveActivityAndWatch(messenger: messenger)

    // Šířka pod desktop shell (rail ≥720 + šachovnice + panel); výchozí XIB bývá 800×600 — příliš úzké.
    minSize = NSSize(width: 940, height: 640)
    var frame = self.frame
    frame.size = NSSize(width: 1320, height: 860)
    setFrame(frame, display: true)
    center()

    super.awakeFromNib()
  }
}
