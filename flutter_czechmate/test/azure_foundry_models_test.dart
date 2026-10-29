import 'dart:convert';

import 'package:czechmate/core/services/coach_ai_completion_service.dart';
import 'package:czechmate/core/services/coach_llm_clients.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:http/http.dart' as http;

void main() {
  test('parseModelIds keeps deployment ids and drops failed or non-chat models', () {
    final body = jsonEncode({
      'data': [
        {'id': 'coach-gpt', 'object': 'deployment', 'model': 'gpt-4o', 'status': 'succeeded'},
        {'id': 'dead', 'status': 'failed'},
        {'id': 'text-embedding-3-large', 'status': 'succeeded'},
        {'id': 'coach-gpt', 'status': 'succeeded'},
      ],
    });
    expect(CoachOpenAiClient.parseModelIds(body), ['coach-gpt']);
  });

  test('azure list tries deployments first and uses that id', () async {
    final client = _Scripted([
      (req) {
        expect(req.headers['api-key'], 'secret');
        expect(req.url.path, '/openai/deployments');
        expect(req.url.queryParameters['api-version'], '2024-10-21');
        return http.Response(
          jsonEncode({
            'data': [
              {'id': 'my-deploy', 'model': 'gpt-4o', 'status': 'succeeded'},
            ],
          }),
          200,
        );
      },
    ]);

    final ids = await CoachAiCompletionService.listAzureFoundryModels(
      baseUrl: 'https://demo.openai.azure.com/openai/v1',
      apiKey: 'secret',
      httpClient: client,
    );
    expect(ids, ['my-deploy']);
    expect(client.uris, hasLength(1));
  });

  test('404 on deployments falls through to the v1 models list', () async {
    final client = _Scripted([
      (_) => http.Response('missing', 404),
      (req) {
        expect(req.url.path, '/openai/v1/models');
        return http.Response(
          jsonEncode({
            'data': [
              {'id': 'gpt-4o-mini'},
            ],
          }),
          200,
        );
      },
    ]);

    final ids = await CoachAiCompletionService.listAzureFoundryModels(
      baseUrl: 'https://demo.openai.azure.com',
      apiKey: 'secret',
      httpClient: client,
    );
    expect(ids, ['gpt-4o-mini']);
    expect(client.uris.map((u) => u.path).toList(), [
      '/openai/deployments',
      '/openai/v1/models',
    ]);
  });
}

class _Scripted extends http.BaseClient {
  _Scripted(this._handlers);

  final List<http.Response Function(http.Request)> _handlers;
  final uris = <Uri>[];
  var _i = 0;

  @override
  Future<http.StreamedResponse> send(http.BaseRequest request) async {
    final req = request as http.Request;
    uris.add(req.url);
    final res = _handlers[_i++](req);
    return http.StreamedResponse(
      Stream<List<int>>.value(res.bodyBytes),
      res.statusCode,
      headers: const {'content-type': 'application/json'},
    );
  }
}
