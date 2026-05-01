import 'dart:typed_data';
import 'dart:ui' as flutter_ui;

import 'package:image/image.dart' as img;

/// Converts a Flutter raster to the `image` package (RGBA).
Future<img.Image?> uiImageToPackageImage(flutter_ui.Image raster) async {
  final bd = await raster.toByteData(format: flutter_ui.ImageByteFormat.rawRgba);
  if (bd == null) return null;
  return img.Image.fromBytes(
    width: raster.width,
    height: raster.height,
    bytes: bd.buffer,
    numChannels: 4,
    order: img.ChannelOrder.rgba,
  );
}

/// Animated GIF (duration in 1/100 s per frame).
Uint8List? encodeGifAnimation(
  List<img.Image> frames, {
  int durationHundredths = 42,
}) {
  if (frames.isEmpty) return null;
  final enc = img.GifEncoder(
    delay: durationHundredths,
    repeat: 0,
    samplingFactor: 6,
  );
  enc.addFrame(frames.first);
  for (var i = 1; i < frames.length; i++) {
    enc.addFrame(frames[i], duration: durationHundredths);
  }
  return enc.finish();
}
