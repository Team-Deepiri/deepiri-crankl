"""Shared, executable helpers for the Crankl demonstration notebooks.

* ``ctypes`` for the public C API in ``include/crankl/``; and
* ``subprocess`` for archive-oriented CLI workflows.

All generated artifacts live under the operating system's temporary directory.
"""

from __future__ import annotations

import ctypes as ct
import json
import os
import subprocess
import tempfile
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable

import numpy as np


ROOT = Path(__file__).resolve().parents[1]
ARTIFACT_DIR = Path(tempfile.gettempdir()) / "crankl_notebook_demo"
ARTIFACT_DIR.mkdir(parents=True, exist_ok=True)


def _first_existing(candidates: Iterable[Path], description: str) -> Path:
    for candidate in candidates:
        if candidate.exists():
            return candidate
    paths = "\n".join(f"  - {path}" for path in candidates)
    raise FileNotFoundError(
        f"Could not find {description}. Build Crankl first; checked:\n{paths}"
    )


CLI = _first_existing(
    [ROOT / "build-notebooks" / "crankl", ROOT / "build" / "crankl"],
    "the crankl CLI",
)

if os.name == "nt":
    _library_names = ("crankl.dll",)
elif os.uname().sysname == "Darwin":
    _library_names = ("libcrankl.dylib",)
else:
    _library_names = ("libcrankl.so",)

LIBRARY = _first_existing(
    [
        ROOT / build_dir / library_name
        for build_dir in ("build-notebooks", "build")
        for library_name in _library_names
    ],
    "the libcrankl shared library",
)


class Multivector(ct.Structure):
    """Python mirror of ``crankl_multivector_t``."""

    _fields_ = [
        ("s", ct.c_double),
        ("v", ct.c_double * 3),
        ("b", ct.c_double * 3),
        ("p", ct.c_double),
    ]

    @classmethod
    def create(
        cls,
        scalar: float = 0.0,
        vector: tuple[float, float, float] = (0.0, 0.0, 0.0),
        bivector: tuple[float, float, float] = (0.0, 0.0, 0.0),
        pseudoscalar: float = 0.0,
    ) -> "Multivector":
        return cls(scalar, (ct.c_double * 3)(*vector), (ct.c_double * 3)(*bivector), pseudoscalar)

    def as_array(self) -> np.ndarray:
        return np.array([self.s, *self.v, *self.b, self.p], dtype=np.float64)


class ArchiveMetrics(ct.Structure):
    """Python mirror of ``crankl_archive_metrics_t``."""

    _fields_ = [
        ("n_slots", ct.c_uint64),
        ("depth_min", ct.c_uint32),
        ("depth_max", ct.c_uint32),
        ("scalar_mean", ct.c_double),
        ("scalar_abs_mean", ct.c_double),
        ("trit_density", ct.c_double),
        ("trit_entropy", ct.c_double),
        ("clifford_energy", ct.c_double),
        ("beta1_proxy", ct.c_double),
    ]

    def as_dict(self) -> dict[str, int | float]:
        return {name: getattr(self, name) for name, _ in self._fields_}


def _u64_pointer(words: np.ndarray):
    words = np.ascontiguousarray(words, dtype=np.uint64)
    return words, words.ctypes.data_as(ct.POINTER(ct.c_uint64))


class CranklAPI:
    """Small typed wrapper around the public C API used by the notebooks."""

    def __init__(self, library_path: Path = LIBRARY):
        self.lib = ct.CDLL(str(library_path))
        self._declare_signatures()

    def _declare_signatures(self) -> None:
        lib = self.lib
        lib.crankl_version_string.restype = ct.c_char_p
        lib.crankl_has_avx2.restype = ct.c_int

        lib.crankl_trit_encode.argtypes = [ct.c_int, ct.POINTER(ct.c_uint8)]
        lib.crankl_trit_encode.restype = ct.c_int
        lib.crankl_trit_decode.argtypes = [ct.c_uint8]
        lib.crankl_trit_decode.restype = ct.c_int

        lib.crankl_crank_from_multivector.argtypes = [ct.POINTER(Multivector), ct.c_uint8]
        lib.crankl_crank_from_multivector.restype = ct.c_uint64
        lib.crankl_crank_to_multivector.argtypes = [
            ct.c_uint64,
            ct.POINTER(Multivector),
            ct.POINTER(ct.c_uint8),
        ]
        lib.crankl_decrank_matrix.argtypes = [ct.c_uint64, ct.POINTER(ct.c_double)]

        lib.crankl_clifford_product.argtypes = [
            ct.POINTER(Multivector),
            ct.POINTER(Multivector),
            ct.POINTER(Multivector),
        ]
        lib.crankl_clifford_reversion.argtypes = [
            ct.POINTER(Multivector),
            ct.POINTER(Multivector),
        ]
        lib.crankl_clifford_resonance.argtypes = [ct.c_uint64, ct.c_uint64]
        lib.crankl_clifford_resonance.restype = ct.c_double

        lib.crankl_pack_n_slots.argtypes = [ct.c_size_t]
        lib.crankl_pack_n_slots.restype = ct.c_size_t
        lib.crankl_pack_f32.argtypes = [
            ct.POINTER(ct.c_float),
            ct.c_size_t,
            ct.POINTER(ct.c_uint64),
            ct.c_size_t,
            ct.c_float,
            ct.c_float,
        ]
        lib.crankl_unpack_f32.argtypes = [
            ct.POINTER(ct.c_uint64),
            ct.c_size_t,
            ct.POINTER(ct.c_float),
            ct.c_size_t,
        ]
        lib.crankl_decrank_frobenius_loss.argtypes = [
            ct.c_uint64,
            ct.POINTER(ct.c_float),
        ]
        lib.crankl_decrank_frobenius_loss.restype = ct.c_double

        lib.crankl_turn.argtypes = [ct.POINTER(ct.c_uint64), ct.c_double]
        lib.crankl_turn_toward.argtypes = [
            ct.POINTER(ct.c_uint64),
            ct.c_double,
            ct.POINTER(ct.c_float),
            ct.c_size_t,
        ]
        lib.crankl_peel.argtypes = [ct.POINTER(ct.c_uint64), ct.c_uint32]
        lib.crankl_bind.argtypes = [ct.c_uint64, ct.c_uint64]
        lib.crankl_bind.restype = ct.c_uint64

        lib.crankl_sheaf_beta1_proxy.argtypes = [ct.POINTER(ct.c_uint64), ct.c_size_t]
        lib.crankl_sheaf_beta1_proxy.restype = ct.c_int
        lib.crankl_sheaf_resonance.argtypes = [
            ct.POINTER(ct.c_uint64),
            ct.c_size_t,
            ct.POINTER(ct.c_uint64),
            ct.c_size_t,
        ]
        lib.crankl_sheaf_resonance.restype = ct.c_double
        lib.crankl_sheaf_h0_dim.argtypes = [ct.POINTER(ct.c_uint64), ct.c_size_t]
        lib.crankl_sheaf_h0_dim.restype = ct.c_int
        lib.crankl_sheaf_h1_dim.argtypes = [ct.POINTER(ct.c_uint64), ct.c_size_t]
        lib.crankl_sheaf_h1_dim.restype = ct.c_int
        lib.crankl_sheaf_cohomology.argtypes = [
            ct.POINTER(ct.c_uint64),
            ct.c_size_t,
            ct.POINTER(ct.c_int),
            ct.POINTER(ct.c_int),
        ]
        lib.crankl_sheaf_cohomology.restype = ct.c_int
        lib.crankl_sheaf_resonance_h1.argtypes = [
            ct.POINTER(ct.c_uint64),
            ct.c_size_t,
            ct.POINTER(ct.c_uint64),
            ct.c_size_t,
        ]
        lib.crankl_sheaf_resonance_h1.restype = ct.c_double

        lib.crankl_crank_diff_count.argtypes = [
            ct.POINTER(ct.c_uint64),
            ct.POINTER(ct.c_uint64),
            ct.c_size_t,
        ]
        lib.crankl_crank_diff_count.restype = ct.c_size_t
        lib.crankl_crank_diff_hamming.argtypes = [
            ct.POINTER(ct.c_uint64),
            ct.POINTER(ct.c_uint64),
            ct.c_size_t,
        ]
        lib.crankl_crank_diff_hamming.restype = ct.c_double

        lib.crankl_compute_archive_metrics.argtypes = [
            ct.POINTER(ct.c_uint64),
            ct.c_size_t,
            ct.POINTER(ArchiveMetrics),
        ]

    def version(self) -> str:
        return self.lib.crankl_version_string().decode("utf-8")

    def encode_trit(self, trit: int) -> int:
        """Encode one ternary value (-1, 0, +1) into its two-bit representation."""

        public_constant = {-1: 2, 0: 0, 1: 1}.get(trit)
        if public_constant is None:
            raise ValueError(f"Invalid trit {trit}; expected -1, 0, or +1")
        encoded = ct.c_uint8()
        status = self.lib.crankl_trit_encode(public_constant, ct.byref(encoded))
        if status != 0:
            raise ValueError(f"Invalid trit {trit}; status {status}")
        return int(encoded.value)

    def decode_trit(self, encoded: int) -> int:
        """Decode a two-bit representation into -1, 0, or +1."""

        public_constant = int(self.lib.crankl_trit_decode(encoded))
        return {0: 0, 1: 1, 2: -1}[public_constant]

    def encode(self, mv: Multivector, depth: int = 1) -> int:
        return int(self.lib.crankl_crank_from_multivector(ct.byref(mv), depth))

    def decode(self, word: int) -> tuple[Multivector, int]:
        mv = Multivector()
        depth = ct.c_uint8()
        self.lib.crankl_crank_to_multivector(word, ct.byref(mv), ct.byref(depth))
        return mv, int(depth.value)

    def clifford_product(self, a: Multivector, b: Multivector) -> Multivector:
        result = Multivector()
        self.lib.crankl_clifford_product(ct.byref(a), ct.byref(b), ct.byref(result))
        return result

    def reversion(self, value: Multivector) -> Multivector:
        result = Multivector()
        self.lib.crankl_clifford_reversion(ct.byref(value), ct.byref(result))
        return result

    def resonance(self, a_word: int, b_word: int) -> float:
        return float(self.lib.crankl_clifford_resonance(a_word, b_word))

    def decrank(self, word: int) -> np.ndarray:
        output = np.empty(64, dtype=np.float64)
        self.lib.crankl_decrank_matrix(
            word, output.ctypes.data_as(ct.POINTER(ct.c_double))
        )
        return output.reshape(8, 8)

    def pack(
        self, values: np.ndarray, persistence_weight: float = 0.1, topology_weight: float = 0.01
    ) -> np.ndarray:
        source = np.ascontiguousarray(values, dtype=np.float32).ravel()
        slot_count = int(self.lib.crankl_pack_n_slots(source.size))
        words = np.empty(slot_count, dtype=np.uint64)
        status = self.lib.crankl_pack_f32(
            source.ctypes.data_as(ct.POINTER(ct.c_float)),
            source.size,
            words.ctypes.data_as(ct.POINTER(ct.c_uint64)),
            slot_count,
            persistence_weight,
            topology_weight,
        )
        if status != 0:
            raise RuntimeError(f"crankl_pack_f32 failed with status {status}")
        return words

    def unpack(self, words: np.ndarray) -> np.ndarray:
        owned_words, pointer = _u64_pointer(words)
        output = np.empty(owned_words.size * 64, dtype=np.float32)
        status = self.lib.crankl_unpack_f32(
            pointer,
            owned_words.size,
            output.ctypes.data_as(ct.POINTER(ct.c_float)),
            output.size,
        )
        if status != 0:
            raise RuntimeError(f"crankl_unpack_f32 failed with status {status}")
        return output.reshape(-1, 8, 8)

    def reconstruction_loss(self, word: int, target: np.ndarray) -> float:
        block = np.ascontiguousarray(target, dtype=np.float32).reshape(64)
        return float(
            self.lib.crankl_decrank_frobenius_loss(
                word, block.ctypes.data_as(ct.POINTER(ct.c_float))
            )
        )

    def turn(self, word: int, learning_rate: float = 0.01) -> int:
        mutable_word = ct.c_uint64(word)
        status = self.lib.crankl_turn(ct.byref(mutable_word), learning_rate)
        if status != 0:
            raise RuntimeError(f"crankl_turn failed with status {status}")
        return int(mutable_word.value)

    def turn_toward(self, word: int, target: np.ndarray, learning_rate: float = 0.01) -> int:
        target_block = np.ascontiguousarray(target, dtype=np.float32).reshape(64)
        mutable_word = ct.c_uint64(word)
        status = self.lib.crankl_turn_toward(
            ct.byref(mutable_word),
            learning_rate,
            target_block.ctypes.data_as(ct.POINTER(ct.c_float)),
            target_block.size,
        )
        if status != 0:
            raise RuntimeError(f"crankl_turn_toward failed with status {status}")
        return int(mutable_word.value)

    def peel(self, word: int, layers: int = 1) -> int:
        mutable_word = ct.c_uint64(word)
        status = self.lib.crankl_peel(ct.byref(mutable_word), layers)
        if status != 0:
            raise RuntimeError(f"crankl_peel failed with status {status}")
        return int(mutable_word.value)

    def bind(self, a_word: int, b_word: int) -> int:
        return int(self.lib.crankl_bind(a_word, b_word))

    def beta1_proxy(self, words: np.ndarray) -> int:
        owned_words, pointer = _u64_pointer(words)
        return int(self.lib.crankl_sheaf_beta1_proxy(pointer, owned_words.size))

    def sheaf_resonance(self, a_words: np.ndarray, b_words: np.ndarray) -> float:
        a_owned, a_pointer = _u64_pointer(a_words)
        b_owned, b_pointer = _u64_pointer(b_words)
        return float(
            self.lib.crankl_sheaf_resonance(
                a_pointer, a_owned.size, b_pointer, b_owned.size
            )
        )

    def h0_dim(self, words: np.ndarray) -> int:
        owned_words, pointer = _u64_pointer(words)
        return int(self.lib.crankl_sheaf_h0_dim(pointer, owned_words.size))

    def h1_dim(self, words: np.ndarray) -> int:
        owned_words, pointer = _u64_pointer(words)
        return int(self.lib.crankl_sheaf_h1_dim(pointer, owned_words.size))

    def cohomology(self, words: np.ndarray) -> tuple[int, int]:
        owned_words, pointer = _u64_pointer(words)
        h0 = ct.c_int()
        h1 = ct.c_int()
        status = self.lib.crankl_sheaf_cohomology(
            pointer, owned_words.size, ct.byref(h0), ct.byref(h1)
        )
        if status != 0:
            raise RuntimeError(f"crankl_sheaf_cohomology failed with status {status}")
        return int(h0.value), int(h1.value)

    def resonance_h1(self, a_words: np.ndarray, b_words: np.ndarray) -> float:
        a_owned, a_pointer = _u64_pointer(a_words)
        b_owned, b_pointer = _u64_pointer(b_words)
        return float(
            self.lib.crankl_sheaf_resonance_h1(
                a_pointer, a_owned.size, b_pointer, b_owned.size
            )
        )

    def diff(self, a_words: np.ndarray, b_words: np.ndarray) -> tuple[int, float]:
        count = min(np.size(a_words), np.size(b_words))
        a_owned, a_pointer = _u64_pointer(np.asarray(a_words).ravel()[:count])
        b_owned, b_pointer = _u64_pointer(np.asarray(b_words).ravel()[:count])
        changed = self.lib.crankl_crank_diff_count(a_pointer, b_pointer, count)
        hamming = self.lib.crankl_crank_diff_hamming(a_pointer, b_pointer, count)
        return int(changed), float(hamming)

    def metrics(self, words: np.ndarray) -> dict[str, int | float]:
        owned_words, pointer = _u64_pointer(words)
        metrics = ArchiveMetrics()
        status = self.lib.crankl_compute_archive_metrics(
            pointer, owned_words.size, ct.byref(metrics)
        )
        if status != 0:
            raise RuntimeError(f"crankl_compute_archive_metrics failed with status {status}")
        return metrics.as_dict()


@dataclass(frozen=True)
class DemoFiles:
    """Paths and arrays used by every notebook."""

    source_path: Path
    target_path: Path
    calibration_x_path: Path
    calibration_y_path: Path
    source_blocks: np.ndarray
    target_blocks: np.ndarray
    calibration_x: np.ndarray
    calibration_y: np.ndarray


def example_float_matrices() -> tuple[np.ndarray, np.ndarray]:
    """Return two deterministic source blocks and nearby target blocks."""

    indices = np.arange(8, dtype=np.float32)
    distance = np.abs(indices[:, None] - indices[None, :])

    # A banded operator: strong diagonal, weaker near-neighbor interaction.
    banded_block = 0.65 * np.eye(8, dtype=np.float32)
    banded_block += 0.18 * (distance == 1)
    banded_block -= 0.07 * (distance == 2)

    # A smooth signed coupling block with values small enough to inspect easily.
    row = np.linspace(-0.4, 0.4, 8, dtype=np.float32)
    column = np.linspace(0.3, -0.3, 8, dtype=np.float32)
    coupling_block = np.outer(row, column).astype(np.float32)
    coupling_block += 0.25 * np.eye(8, dtype=np.float32)

    source_blocks = np.stack([banded_block, coupling_block])
    perturbation = 0.025 * np.sin(np.arange(128, dtype=np.float32)).reshape(2, 8, 8)
    target_blocks = source_blocks + perturbation
    return source_blocks, target_blocks.astype(np.float32)


def prepare_demo_files() -> DemoFiles:
    """Create deterministic binary inputs in the temporary demo directory."""

    source_blocks, target_blocks = example_float_matrices()
    calibration_x = np.linspace(-1.0, 1.0, 8, dtype=np.float32)
    calibration_y = (0.75 * calibration_x + 0.1).astype(np.float32)

    source_path = ARTIFACT_DIR / "example_weights.f32"
    target_path = ARTIFACT_DIR / "example_target.f32"
    calibration_x_path = ARTIFACT_DIR / "calibration_x.f32"
    calibration_y_path = ARTIFACT_DIR / "calibration_y.f32"

    source_blocks.tofile(source_path)
    target_blocks.tofile(target_path)
    calibration_x.tofile(calibration_x_path)
    calibration_y.tofile(calibration_y_path)

    return DemoFiles(
        source_path,
        target_path,
        calibration_x_path,
        calibration_y_path,
        source_blocks,
        target_blocks,
        calibration_x,
        calibration_y,
    )


def run_cli(*arguments: object, expect_json: bool = False):
    """Run the built CLI, print the command, and return text or parsed JSON."""

    command = [str(CLI), *(str(argument) for argument in arguments)]
    print("$", " ".join(command))
    completed = subprocess.run(
        command,
        cwd=ROOT,
        check=True,
        text=True,
        capture_output=True,
    )
    output = completed.stdout.strip()
    if output:
        print(output)
    return json.loads(output) if expect_json else output


def print_matrix(name: str, matrix: np.ndarray) -> None:
    """Print an 8×8 block with stable, readable formatting."""

    print(f"\n{name} (shape={matrix.shape}, min={matrix.min():.4f}, max={matrix.max():.4f})")
    print(np.array2string(np.asarray(matrix), precision=3, suppress_small=True))

