"""Offline SSRF regression for bili short-link resolution (no network)."""
import asyncio
import importlib.util
from pathlib import Path
from types import SimpleNamespace
import unittest
from unittest.mock import AsyncMock, patch

spec = importlib.util.spec_from_file_location('selftest_loader', Path(__file__).resolve().parents[2] / 'plugin-selftest.py')
loader = importlib.util.module_from_spec(spec)
spec.loader.exec_module(loader)
m = loader.load_plugin()


class FakeResponse:
    def __init__(self, status, url, headers=None, delay=0.0):
        self.status = status
        self.url = url
        self.headers = headers or {}
        self.delay = delay
        self.closed = False

    def release(self):
        self.closed = True


class FakeResponseContext:
    def __init__(self, response):
        self.response = response

    async def __aenter__(self):
        if self.response.delay:
            await asyncio.sleep(self.response.delay)
        return self.response

    async def __aexit__(self, exc_type, exc, tb):
        self.response.closed = True
        return False


class FakeSession:
    def __init__(self, responses):
        self.responses = list(responses)
        self.calls = []
        self.exited = False

    def get(self, url, **kwargs):
        self.calls.append((url, kwargs))
        if not self.responses:
            raise AssertionError('unexpected extra request: ' + url)
        return FakeResponseContext(self.responses.pop(0))

    async def __aenter__(self):
        return self

    async def __aexit__(self, exc_type, exc, tb):
        self.exited = True
        return False


class FakeAiohttp:
    def __init__(self, responses=(), delay=0.0):
        self.responses = list(responses)
        self.delay = delay
        self.sessions = []

    def ClientTimeout(self, **kwargs):
        return kwargs

    def ClientSession(self):
        session = FakeSession(self.responses)
        self.sessions.append(session)
        return session


def response(status, location=None, url='https://www.bilibili.com/video/BV1o64y167qq', delay=0.0):
    return FakeResponse(status, url, {} if location is None else {'Location': location}, delay)


class ValidationTests(unittest.TestCase):
    def test_local_and_redirect_url_rules(self):
        self.assertEqual(m._validate_bili_url('https://b23.tv/abc12'), 'short')
        self.assertEqual(m._validate_bili_url('https://www.bilibili.com/video/BV1o64y167qq'), 'video')
        self.assertEqual(m._validate_bili_url('http://bilibili.com/video/av170001'), 'video')
        self.assertEqual(m._validate_bili_url('https://b23.tv/abc123?x=@'), 'short')
        self.assertIsNone(m._validate_bili_url('https://@b23.tv/abc123'))
        self.assertIsNone(m._validate_bili_url('https://user:pass@b23.tv/abc123'))
        self.assertIsNone(m._validate_bili_url('http://169.254.169.254/latest/meta-data/'))
        self.assertIsNone(m._validate_bili_url('https://b23.tv.evil.example/abc123'))
        self.assertIsNone(m._validate_bili_url('https://b23.tv/abc'))
        self.assertIsNone(m._validate_bili_url('https://b23.tv/abcdefghijklm'))
        self.assertIsNone(m._validate_bili_url('https://b23.tv:8080/abc123'))
        self.assertIsNone(m._validate_bili_url('https://b23.tv:bad/abc123'))
        self.assertIsNone(m._validate_bili_url('https://b23.tv:65536/abc123'))
        self.assertIsNone(m._validate_bili_url('https://[::1'))
        self.assertIsNone(m._validate_bili_url('http://[::1]/'))
        self.assertIsNone(m._validate_bili_url('file:///C:/Windows/win.ini'))
        self.assertEqual(m._validate_bili_url('https://www.bilibili.com/video/BV1o64y167qq', 'https'), 'video')

    def test_nfkc_and_unicode_hosts_rejected(self):
        for raw in (
            'https://b\uff12\uff13.tv/abc123',
            'https://b23\u3002tv/abc123',
            'https://\uff22\uff12\uff13.\uff34\uff36/abc123',
            'https://b23.tv\u3002evil.example/abc123',
        ):
            self.assertIsNone(m._validate_bili_url(raw), raw)

    def test_https_downgrade_is_rejected(self):
        self.assertIsNone(m._validate_bili_url('http://www.bilibili.com/video/BV1o64y167qq', 'https'))
        self.assertEqual(m._validate_bili_url('https://www.bilibili.com/video/BV1o64y167qq', 'https'), 'video')


class ResolveTests(unittest.IsolatedAsyncioTestCase):
    async def asyncSetUp(self):
        self.bridge = m.BiliBridge(None, {'owner_session': '', 'persona_id': '冥零'})

    async def resolve(self, text, responses=(), delay=0.0):
        fake = FakeAiohttp(responses, delay)
        with patch.object(m, 'aiohttp', fake):
            result = await self.bridge._resolve_bv(text)
        return result, fake

    async def test_bv_inside_foreign_url_is_not_used(self):
        result, fake = await self.resolve('https://evil.example/BV1o64y167qq/x')
        self.assertEqual(result, '')
        self.assertEqual(fake.sessions, [])
        # 纯文本里夹带的 BV 仍按历史行为直接提取
        result, fake = await self.resolve('ok BV1o64y167qq ok')
        self.assertEqual(result, 'BV1o64y167qq')
        self.assertEqual(fake.sessions, [])

    async def test_local_bv_and_full_link_need_no_network(self):
        for text in ('BV1o64y167qq', 'https://www.bilibili.com/video/BV1o64y167qq?p=2'):
            result, fake = await self.resolve(text)
            self.assertEqual(result, 'BV1o64y167qq')
            self.assertEqual(fake.sessions, [])

    async def test_short_link_redirect_success_releases_resources(self):
        responses = [
            response(302, 'https://www.bilibili.com/video/BV1o64y167qq'),
            response(200, url='https://www.bilibili.com/video/BV1o64y167qq'),
        ]
        result, fake = await self.resolve('https://b23.tv/abc123', responses)
        self.assertEqual(result, 'BV1o64y167qq')
        self.assertEqual(len(fake.sessions), 1)
        self.assertEqual(len(fake.sessions[0].calls), 2)
        self.assertTrue(all(r.closed for r in responses))
        self.assertTrue(fake.sessions[0].exited)

    async def test_multi_hop_short_links_still_work(self):
        responses = [
            response(302, 'https://b23.tv/def34', url='https://b23.tv/abc12'),
            response(302, 'https://www.bilibili.com/video/BV1o64y167qq', url='https://b23.tv/def34'),
            response(200, url='https://www.bilibili.com/video/BV1o64y167qq'),
        ]
        result, fake = await self.resolve('https://b23.tv/abc12', responses)
        self.assertEqual(result, 'BV1o64y167qq')
        self.assertEqual(len(fake.sessions[0].calls), 3)

    async def test_redirect_to_private_or_foreign_host_is_refused(self):
        for location in ('http://127.0.0.1:6199/api/bridge-token', 'https://evil.example/x'):
            responses = [response(302, location)]
            result, fake = await self.resolve('https://b23.tv/abc123', responses)
            self.assertEqual(result, '')
            self.assertEqual(len(fake.sessions[0].calls), 1)
            self.assertTrue(fake.sessions[0].exited)

    async def test_https_downgrade_location_is_refused(self):
        responses = [response(302, 'http://www.bilibili.com/video/BV1o64y167qq')]
        result, fake = await self.resolve('https://b23.tv/abc123', responses)
        self.assertEqual(result, '')
        self.assertEqual(len(fake.sessions[0].calls), 1)

    async def test_relative_location_is_revalidated(self):
        # 相对路径解析后仍是 b23.tv 的非法路径，必须就地拒绝
        responses = [response(302, '/video/BV1o64y167qq')]
        result, fake = await self.resolve('https://b23.tv/abc123', responses)
        self.assertEqual(result, '')
        self.assertEqual(len(fake.sessions[0].calls), 1)

    async def test_relative_location_resolving_to_short_link_is_followed(self):
        responses = [
            response(302, 'abc123'),
            response(302, 'https://www.bilibili.com/video/BV1o64y167qq'),
            response(200, url='https://www.bilibili.com/video/BV1o64y167qq'),
        ]
        result, fake = await self.resolve('https://b23.tv/abc123', responses)
        self.assertEqual(result, 'BV1o64y167qq')
        self.assertEqual(len(fake.sessions[0].calls), 3)

    async def test_protocol_relative_location_is_validated(self):
        responses = [
            response(302, '//www.bilibili.com/video/BV1o64y167qq'),
            response(200, url='https://www.bilibili.com/video/BV1o64y167qq'),
        ]
        result, fake = await self.resolve('https://b23.tv/abc123', responses)
        self.assertEqual(result, 'BV1o64y167qq')
        self.assertEqual(len(fake.sessions[0].calls), 2)

    async def test_location_with_invalid_port_is_refused(self):
        for location in ('https://b23.tv:8443/abc123', 'https://b23.tv:bad/abc123'):
            responses = [response(302, location)]
            result, fake = await self.resolve('https://b23.tv/abc123', responses)
            self.assertEqual(result, '', location)
            self.assertEqual(len(fake.sessions[0].calls), 1)

    async def test_private_file_and_malformed_inputs_never_create_session(self):
        for text in (
            'http://169.254.169.254/latest/meta-data/',
            'file:///C:/Windows/win.ini',
            'https://[::1',
            'https://b23.tv:bad/abc123',
            'https://b23.tv:65536/abc123',
        ):
            result, fake = await self.resolve(text)
            self.assertEqual(result, '')
            self.assertEqual(fake.sessions, [])

    async def test_redirect_loop_uses_exactly_four_requests(self):
        responses = [
            response(302, 'https://b23.tv/loop1'),
            response(302, 'https://b23.tv/loop2'),
            response(302, 'https://b23.tv/loop3'),
            response(302, 'https://b23.tv/loop4'),
        ]
        result, fake = await self.resolve('https://b23.tv/abc123', responses)
        self.assertEqual(result, '')
        self.assertEqual(len(fake.sessions[0].calls), 4)
        self.assertTrue(fake.sessions[0].exited)

    async def test_fourth_non_redirect_response_succeeds(self):
        responses = [
            response(302, 'https://b23.tv/def34'),
            response(302, 'https://b23.tv/ghi56'),
            response(302, 'https://www.bilibili.com/video/BV1o64y167qq'),
            response(200, url='https://www.bilibili.com/video/BV1o64y167qq'),
        ]
        result, fake = await self.resolve('https://b23.tv/abc12', responses)
        self.assertEqual(result, 'BV1o64y167qq')
        self.assertEqual(len(fake.sessions[0].calls), 4)

    async def test_missing_location_and_no_bv_fail_cleanly(self):
        for responses in ([response(302)], [response(200, url='https://b23.tv/abc123')]):
            result, fake = await self.resolve('https://b23.tv/abc123', responses)
            self.assertEqual(result, '')
            self.assertTrue(fake.sessions[0].exited)

    async def test_network_error_returns_empty_and_closes_session(self):
        fake = FakeAiohttp([RuntimeError('boom')])

        class FailingContext(FakeResponseContext):
            async def __aenter__(self):
                raise self.response

        original = fake.ClientSession

        def failing_session():
            session = original()
            session.get = lambda *a, **k: FailingContext(FakeResponse(200, 'x'))
            return session

        fake.ClientSession = failing_session
        with patch.object(m, 'aiohttp', fake):
            result = await self.bridge._resolve_bv('https://b23.tv/abc123')
        self.assertEqual(result, '')
        self.assertTrue(fake.sessions[0].exited)

    async def test_shared_deadline_is_not_reset_per_hop(self):
        responses = [
            response(302, 'https://www.bilibili.com/video/BV1o64y167qq', delay=0.25),
            response(200, url='https://www.bilibili.com/video/BV1o64y167qq'),
        ]
        result, fake = await self.resolve('https://b23.tv/abc123', responses)
        self.assertEqual(result, 'BV1o64y167qq')
        first = fake.sessions[0].calls[0][1]['timeout']['total']
        second = fake.sessions[0].calls[1][1]['timeout']['total']
        self.assertLess(second, first - 0.2)
        self.assertLess(second, m.RIFT_RESOLVE_TIMEOUT)

    async def test_plain_av_uses_fixed_api_without_session(self):
        with patch.object(self.bridge, '_av_to_bv', AsyncMock(return_value='BV1o64y167qq')) as api:
            fake = FakeAiohttp()
            with patch.object(m, 'aiohttp', fake):
                result = await self.bridge._resolve_bv('av170001')
        self.assertEqual(result, 'BV1o64y167qq')
        api.assert_awaited_once_with('170001')
        self.assertEqual(fake.sessions, [])

    async def test_valid_av_page_uses_only_fixed_api(self):
        with patch.object(self.bridge, '_av_to_bv', AsyncMock(return_value='BV1o64y167qq')) as api:
            fake = FakeAiohttp()
            with patch.object(m, 'aiohttp', fake):
                result = await self.bridge._resolve_bv('https://www.bilibili.com/video/av170001')
        self.assertEqual(result, 'BV1o64y167qq')
        api.assert_awaited_once_with('170001')
        self.assertEqual(fake.sessions, [])

    async def test_invalid_url_containing_av_never_networks(self):
        with patch.object(self.bridge, '_av_to_bv', AsyncMock(return_value='BV1o64y167qq')) as api:
            fake = FakeAiohttp()
            with patch.object(m, 'aiohttp', fake):
                result = await self.bridge._resolve_bv('http://127.0.0.1/av123')
        self.assertEqual(result, '')
        api.assert_not_called()
        self.assertEqual(fake.sessions, [])

    async def test_unresolved_text_is_fixed_chinese(self):
        bridge = self.bridge
        said = []

        async def fake_say(umo, text):
            said.append(text)

        with patch.object(bridge, '_say', fake_say), patch.object(bridge, '_resolve_bv', AsyncMock(return_value='')):
            await bridge._handle('umo', 'chat', '', 'https://b23.tv/abc123')
        self.assertEqual(said, ['这个链接我追不到 BV 号，你直接贴 BV 吧。'])
        self.assertNotIn('abc123', ''.join(said))

    async def test_error_does_not_expose_internal_details(self):
        result, fake = await self.resolve('https://b23.tv/abc123', [response(302, 'http://127.0.0.1:6199/x')])
        self.assertEqual(result, '')


if __name__ == '__main__':
    unittest.main()
