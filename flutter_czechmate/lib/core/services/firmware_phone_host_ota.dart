import 'dart:io';

import 'package:http/http.dart' as http;
import 'package:path_provider/path_provider.dart';

/// Lokální HTTP hostování .bin pro [ota_update.c] HTTP OTA (deska na AP stáhne z aplikace na tomto zařízení).
class FirmwarePhoneHostOta {
  FirmwarePhoneHostOta._();

  static const String _servePath = '/czechmate_ota.bin';

  /// Trvalé uložení (ApplicationSupport) — přežije odpojení desky i pozastavení aplikace.
  static Future<File> downloadBinForOta({
    required String httpsBinUrl,
    required String version,
  }) async {
    final res = await http.get(Uri.parse(httpsBinUrl));
    if (res.statusCode != 200) {
      throw StateError('HTTP ${res.statusCode}');
    }
    final dir = await getApplicationSupportDirectory();
    final firmwareDir = Directory('${dir.path}/firmware_cache');
    if (!await firmwareDir.exists()) {
      await firmwareDir.create(recursive: true);
    }
    final safe = version.replaceAll(RegExp(r'[^0-9a-zA-Z._-]'), '_');
    final f = File('${firmwareDir.path}/czechmate_ota_$safe.bin');
    await f.writeAsBytes(res.bodyBytes, flush: true);
    return f;
  }

  @Deprecated('Use downloadBinForOta — temp dir may be cleared by the OS.')
  static Future<File> downloadBinToTemp({
    required String httpsBinUrl,
    required String version,
  }) =>
      downloadBinForOta(httpsBinUrl: httpsBinUrl, version: version);

  /// Klient na hotspotu desky typicky dostane 192.168.4.x.
  static Future<String?> ipv4OnBoardApSubnet() async {
    try {
      for (final iface in await NetworkInterface.list()) {
        for (final addr in iface.addresses) {
          if (addr.type == InternetAddressType.IPv4 &&
              addr.address.startsWith('192.168.4.')) {
            return addr.address;
          }
        }
      }
    } catch (_) {}
    return null;
  }

  /// IPv4 tohoto zařízení ve stejném /24 prefixu jako STA IP desky (domácí LAN).
  static Future<String?> ipv4OnSameSubnet24As(String boardStaIp) async {
    final parts = boardStaIp.trim().split('.');
    if (parts.length != 4) return null;
    final a = int.tryParse(parts[0]);
    final b = int.tryParse(parts[1]);
    final c = int.tryParse(parts[2]);
    if (a == null || b == null || c == null) return null;
    final prefix = '$a.$b.$c';
    try {
      for (final iface in await NetworkInterface.list()) {
        for (final addr in iface.addresses) {
          if (addr.type == InternetAddressType.IPv4 &&
              addr.address.startsWith('$prefix.')) {
            return addr.address;
          }
        }
      }
    } catch (_) {}
    return null;
  }

  /// Vrátí server (je potřeba po OTA [close]) a URL pro `POST /api/system/ota` / BLE `ota_start`.
  ///
  /// [boardStaIpForSubnet] — pokud toto zařízení není na 192.168.4.x, zkusí najít lokální IP ve stejné /24.
  static Future<({HttpServer server, String otaUrl})> startServingBin(
    File bin, {
    String? boardStaIpForSubnet,
  }) async {
    var ip = await ipv4OnBoardApSubnet();
    if (ip == null &&
        boardStaIpForSubnet != null &&
        boardStaIpForSubnet.trim().isNotEmpty) {
      ip = await ipv4OnSameSubnet24As(boardStaIpForSubnet.trim());
    }
    if (ip == null) {
      throw StateError('NOT_ON_OTA_LAN');
    }
    /* Na iOS (a více rozhraních) je spolehlivější naslouchat přímo na IP hotspotu/LAN,
     * aby deska vždy tahala z adresy v otaUrl. Fallback: libovolné rozhraní. */
    HttpServer server;
    try {
      server = await HttpServer.bind(InternetAddress(ip), 0);
    } catch (_) {
      server = await HttpServer.bind(InternetAddress.anyIPv4, 0);
    }
    final port = server.port;
    server.listen((HttpRequest req) async {
      try {
        if (req.uri.path != _servePath || req.method != 'GET') {
          req.response.statusCode = HttpStatus.notFound;
          await req.response.close();
          return;
        }
        final length = await bin.length();
        req.response.statusCode = HttpStatus.ok;
        req.response.headers.contentType =
            ContentType('application', 'octet-stream');
        req.response.headers.set(HttpHeaders.contentLengthHeader, '$length');
        await req.response.addStream(bin.openRead());
        await req.response.close();
      } catch (_) {
        try {
          req.response.statusCode = HttpStatus.internalServerError;
          await req.response.close();
        } catch (_) {}
      }
    });
    return (server: server, otaUrl: 'http://$ip:$port$_servePath');
  }
}
