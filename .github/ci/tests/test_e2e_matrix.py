"""The native E2E matrix must start both roles of a pair together.

cross-platform-e2e.yml caps the matrix with max-parallel, and GitHub starts
capped entries in the order listed. Role 1 immediately followed by role 2 of
the same scenario keeps a waiting role's partner next in line; any other
order can leave every slot held by a role whose partner never starts.
"""

import pathlib
import re
import unittest

WORKFLOW = (pathlib.Path(__file__).resolve().parents[2]
            / "workflows" / "cross-platform-e2e.yml")
ENTRY = re.compile(r"^\s*-\s*\{.*\bscenario:\s*([\w-]+),\s*role:\s*(\d)\b", re.MULTILINE)
MAX_PARALLEL = re.compile(r"^\s*max-parallel:\s*(\d+)\s*$", re.MULTILINE)


class E2EMatrix(unittest.TestCase):
    def setUp(self):
        self.text = WORKFLOW.read_text(encoding="utf-8")
        self.entries = ENTRY.findall(self.text)

    def test_roles_of_a_pair_are_adjacent(self):
        self.assertTrue(self.entries, "no matrix entries found")
        self.assertEqual(len(self.entries) % 2, 0)
        for index in range(0, len(self.entries), 2):
            first, second = self.entries[index], self.entries[index + 1]
            with self.subTest(pair=first[0]):
                self.assertEqual((first[1], second[1]), ("1", "2"))
                self.assertEqual(first[0], second[0])

    def test_scenarios_are_unique(self):
        scenarios = [scenario for scenario, role in self.entries if role == "1"]
        self.assertEqual(len(scenarios), len(set(scenarios)))

    def test_parallelism_is_capped_even_and_below_the_account_limit(self):
        # One cap per matrix job (the main matrix and the macOS one); their
        # sum stays below the 20-job account limit.
        caps = [int(value) for value in MAX_PARALLEL.findall(self.text)]
        self.assertEqual(len(caps), 2)
        for cap in caps:
            self.assertEqual(cap % 2, 0)
        self.assertLess(sum(caps), 20)

    def test_macos_pairs_leave_a_macos_runner_free(self):
        # The account runs five macOS jobs at once. Worst case for the macOS
        # matrix: its cap's worth of pairs, choosing the pairs with the most
        # macOS roles (macos-macos has two). Keep it below five so at least
        # one runner stays free for other macOS work.
        section = self.text[self.text.index("  execute-macos:"):]
        section = section[:section.index("    steps:")]
        cap = int(MAX_PARALLEL.search(section).group(1))
        per_pair = {}
        for line in re.findall(r"^\s*-\s*\{.*\}$", section, re.MULTILINE):
            scenario = re.search(r"\bscenario:\s*([\w-]+)", line).group(1)
            per_pair[scenario] = per_pair.get(scenario, 0) + ("os: macos-" in line)
        worst = sum(sorted(per_pair.values(), reverse=True)[:cap // 2])
        self.assertTrue(per_pair)
        self.assertLessEqual(worst, 4)

if __name__ == "__main__":
    unittest.main()
