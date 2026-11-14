"""Offline Python checks for the singing service's in-process pin gate."""
import hashlib
import importlib.util
import json
import tempfile
import unittest
from pathlib import Path

SERVER = Path(__file__).resolve().parents[2] / 'pack' / 'mingling-pack' / 'plugins' / 'astrbot_plugin_dafeiyu_sing' / 'sing_server' / 'sing_server.py'
spec = importlib.util.spec_from_file_location('mingling_sing_server', SERVER)
m = importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)

PATCHED_LOADER = 'checkpoint = torch.load(model_path, weights_only=True, map_location="cpu")\n'
COMPONENTS = Path(__file__).resolve().parents[2] / 'tools' / 'components.json'


class SingingPinGateTests(unittest.TestCase):
    def setUp(self):
        # 夹具用合成模型/加载器，所以把锚点临时改成夹具的哈希；锚点本身由
        # test_anchors_match_components 与 components.json 对表。
        self._saved = (m._EXPECTED_MODEL_SHA256, m._EXPECTED_LOADER_SHA256)
        m._EXPECTED_MODEL_SHA256 = hashlib.sha256(b'fixture-model').hexdigest()
        m._EXPECTED_LOADER_SHA256 = hashlib.sha256(PATCHED_LOADER.encode('utf-8')).hexdigest()
        self.tmp = tempfile.TemporaryDirectory()
        root = Path(self.tmp.name)
        source = root / 'SoulX-Singer'
        (source / 'cli').mkdir(parents=True)
        model = root / 'model.pt'
        model.write_bytes(b'fixture-model')
        loader = source / 'cli' / 'inference.py'
        loader.write_bytes(PATCHED_LOADER.encode('utf-8'))
        marker = {
            'sourceCommit': '81aeb3ae772c70093c3de74dc23c92d983801ae4',
            'archiveSha256': 'b1a1e9615f8832012396aa306a5de5a6157f4c193c4ce08b3ee35a7983dcee7b',
            'loaderSha256': hashlib.sha256(PATCHED_LOADER.encode()).hexdigest(),
        }
        (source / '.mingling-source.json').write_text(json.dumps(marker), encoding='utf-8')
        installed = {
            'sourceCommit': marker['sourceCommit'],
            'modelSha256': hashlib.sha256(model.read_bytes()).hexdigest(),
            'modelHashSource': 'manifest',
            'loaderSha256': marker['loaderSha256'],
        }
        self.installed_path = root / 'installed.json'
        self.installed_path.write_text(json.dumps(installed), encoding='utf-8')
        self.source = source
        self.model = model
        self.loader = loader
        self.marker = marker
        self.installed = installed

    def tearDown(self):
        m._EXPECTED_MODEL_SHA256, m._EXPECTED_LOADER_SHA256 = self._saved
        self.tmp.cleanup()

    def test_anchors_match_components(self):
        m._EXPECTED_MODEL_SHA256, m._EXPECTED_LOADER_SHA256 = self._saved
        manifest = json.loads(COMPONENTS.read_text(encoding='utf-8-sig'))
        component = next(c for c in manifest['components'] if c['id'] == 'soulx-singer')
        self.assertEqual(m._EXPECTED_SOURCE_COMMIT, component['source']['commit'])
        self.assertEqual(m._EXPECTED_MODEL_SHA256, component['model']['sha256'])
        self.assertEqual(m._EXPECTED_LOADER_SHA256, component['source']['loaderSha256'])
        self.assertRegex(m._EXPECTED_LOADER_SHA256, r'^[0-9a-f]{64}$')

    def test_self_consistent_forgery_is_rejected(self):
        forged = 'deadbeef' * 8
        marker = dict(self.marker)
        marker['sourceCommit'] = forged
        (self.source / '.mingling-source.json').write_text(json.dumps(marker), encoding='utf-8')
        installed = dict(self.installed)
        installed['sourceCommit'] = forged
        self.installed_path.write_text(json.dumps(installed), encoding='utf-8')
        with self.assertRaises(RuntimeError):
            m._verify_soulx_pins(str(self.source), str(self.model), str(self.installed_path))

    def test_valid_pins_pass(self):
        m._verify_soulx_pins(str(self.source), str(self.model), str(self.installed_path))

    def test_tampered_model_is_rejected(self):
        self.model.write_bytes(b'tampered-model')
        with self.assertRaises(RuntimeError):
            m._verify_soulx_pins(str(self.source), str(self.model), str(self.installed_path))

    def test_tampered_loader_is_rejected(self):
        self.loader.write_text(PATCHED_LOADER + '\nextra = 1\n', encoding='utf-8')
        with self.assertRaises(RuntimeError):
            m._verify_soulx_pins(str(self.source), str(self.model), str(self.installed_path))

    def test_unsafe_loader_is_rejected(self):
        self.loader.write_text(PATCHED_LOADER.replace('weights_only=True', 'weights_only=False'), encoding='utf-8')
        with self.assertRaises(RuntimeError):
            m._verify_soulx_pins(str(self.source), str(self.model), str(self.installed_path))

    def test_missing_installed_record_is_rejected(self):
        with self.assertRaises(RuntimeError):
            m._verify_soulx_pins(str(self.source), str(self.model), str(self.installed_path.with_name('missing.json')))

    def test_load_soulx_verifies_before_model_import(self):
        class Args:
            soulx = str(self.source)
            model = str(self.model)
            device = 'cpu'

        def fail(*args):
            raise RuntimeError('pin gate failed')

        original = m._verify_soulx_pins
        m._verify_soulx_pins = fail
        try:
            with self.assertRaises(RuntimeError):
                m.load_soulx(Args)
        finally:
            m._verify_soulx_pins = original


if __name__ == '__main__':
    unittest.main()
