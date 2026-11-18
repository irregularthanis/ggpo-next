"""Path-to-area rules: the cases that decide what CI runs."""

import pathlib
import sys
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))
import affected_areas as a  # noqa: E402

CORE = set(a.CORE)


class Rules(unittest.TestCase):
    def check(self, path, expected):
        with self.subTest(path=path):
            self.assertEqual(a.areas_for(path), set(expected))

    def test_documentation_runs_nothing(self):
        for path in ("doc/architecture/Testing.md", "README.md", "services/matchmaking/README.md",
                     "infra/terraform/README.md"):
            self.check(path, ())

    def test_shared_core_runs_every_platform(self):
        for path in ("src/lib/ggpo/sync.cpp", "src/lib/ggpo/network/udp.cpp",
                     "src/include/ggponet.h", "CMakeLists.txt", "src/cmake/GGPOConfig.cmake.in"):
            self.check(path, CORE)

    def test_os_adapters_run_their_family(self):
        self.check("src/lib/ggpo/platform_posix.cpp", ("linux", "android", "macos", "e2e", "nat"))
        self.check("src/lib/ggpo/platform_windows.cpp", ("windows", "mingw", "e2e"))
        self.check("src/platform/android/Platform.cmake", ("android", "e2e"))
        self.check("src/platform/apple/Platform.cmake", ("macos",))
        self.check("scripts/macos/audit_binaries.py", ("macos",))

    def test_vector_war_frontends(self):
        self.check("src/apps/vectorwar/win32_start_dialog.cpp", ("windows",))
        self.check("src/apps/vectorwar/VectorWar.rc", ("windows",))
        self.check("src/apps/vectorwar/sdl_start_screen.cpp", ("linux", "android", "macos"))
        self.check("src/apps/vectorwar/macos/Info.plist.in", ("macos",))
        self.check("src/apps/vectorwar/android/app/build.gradle", ("android",))
        self.check("src/apps/vectorwar/gamestate.cpp", CORE)
        self.check("src/apps/vectorwar/vw_legend.cpp", CORE)

    def test_server_and_infrastructure(self):
        self.check("services/matchmaking/matchmaking/relay.py", ("infra", "nat", "linux"))
        self.check("infra/terraform/modules/host/main.tf", ("infra",))

    def test_tests(self):
        self.check("tests/android/run_on_device.py", ("android",))
        self.check("tests/nat/topology.sh", ("nat",))
        self.check("tests/unit/udp_send_test.cpp", CORE)

    def test_workflows_run_their_own_area(self):
        self.check(".github/workflows/windows-compatibility.yml", ("windows",))
        self.check(".github/workflows/macos-compatibility.yml", ("macos",))
        self.check(".github/workflows/deploy-matchmaking.yml", ("infra",))
        self.check(".github/workflows/owner-approval.yml", ())

    def test_classifier_and_unknown_paths_run_everything(self):
        for path in (".github/workflows/classify-changes.yml", ".github/ci/affected_areas.py",
                     ".github/workflows/brand-new.yml", "bin/something", "newdir/file.c"):
            self.check(path, a.AREAS)

    def test_union_over_several_paths(self):
        self.assertEqual(a.affected(["src/apps/vectorwar/win32_host.cpp", "infra/x.tf", "doc/a.md"]),
                         {"windows", "infra"})
        self.assertEqual(a.affected(["doc/a.md"]), set())


class EveryTrackedPathIsClassified(unittest.TestCase):
    """Only repository-wide files may fall back to running every area. A new
    directory or file type must get an explicit rule, or be added here."""

    EVERYTHING_ON_PURPOSE = {".dockerignore", ".github/CODEOWNERS", ".gitignore",
                             "LICENSE", "build/.gitignore"}

    def test_repository(self):
        import fnmatch
        import subprocess
        import shutil
        root = pathlib.Path(__file__).resolve().parents[3]
        # Some test environments (MSYS2 MinGW, extracted build trees) have no
        # git or no checkout; the Linux and Windows lanes still run this scan.
        if shutil.which("git") is None or not (root / ".git").exists():
            self.skipTest("git or a git checkout is not available here")
        files = subprocess.run(["git", "-C", str(root), "ls-files"], check=True,
                               capture_output=True, text=True).stdout.split()
        unmatched = {f for f in files
                     if not f.startswith(".github/workflows/")
                     and not any(any(fnmatch.fnmatchcase(f, p) for p in patterns)
                                 for patterns, _ in a.RULES)}
        self.assertEqual(unmatched - self.EVERYTHING_ON_PURPOSE, set())

    def test_every_workflow_file_has_an_area(self):
        root = pathlib.Path(__file__).resolve().parents[2]
        names = {p.name for p in (root / "workflows").glob("*.yml")} - {"classify-changes.yml"}
        self.assertEqual(names - set(a.WORKFLOW_AREAS), {"release-packages.yml"})


if __name__ == "__main__":
    unittest.main()
