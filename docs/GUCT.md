# Grand Unified Crank Theory (GUCT)

Crankle treats each vector slot not as a scalar float but as a **collapsed crank** — a
64-bit word encoding a Clifford multivector over ternary generators {-1, 0, +1}.

## Pillars

1. **Clifford crank** — geometric product decrankling; resonance via Clifford inner product.
2. **Sheaf coboundary** — restriction maps between slots; β₁ proxy for topological similarity.
3. **Symplectic Turn** — Lie-group evolution on crank space instead of float gradient descent.
4. **RG depth** — stacked layers as UV→IR coarse-graining; peel integrates out high-frequency modes.
5. **Persistent packing** — fold float matrices minimizing Frobenius + persistence Wasserstein proxy.
6. **Holonomy** — path-ordered product of decrankled operators as forward pass.

## Crank word layout (64 bits)

| Field | Bits | Role |
|-------|------|------|
| scalar | 16 | fixed-point Clifford scalar α₀ |
| vectors | 12 | e₁,e₂,e₃ trits |
| bivectors | 18 | six bivector blades |
| trivector | 6 | e₁₂₃ pseudoscalar pattern |
| depth | 8 | RG stack depth |
| flags | 4 | version / hints |

## Operations

- **pack** — minimize J(C) = ‖W − decrank(C)‖² + λ·W₂(PD) + μ·β₁
- **resonance** — ⟨C₁, C₂⟩_Cl and sheaf χ proxy
- **turn** — symplectic Verlet step + BCH trit projection
- **peel** — remove top RG layers
- **bind** — Clifford product of crank tensors
- **holonomy** — ∏ exp(iγ·decrank(Cᵢ)) · x

See notebooks `00`–`07` for derivations and golden references.
