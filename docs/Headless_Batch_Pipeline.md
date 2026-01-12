# Headless Batch Pipeline (Simulation -> Report)

Minimal, repeatable pipeline to run simulations headless and generate reports.

## Example (Windows PowerShell)

```powershell
.\micro_swarm.exe `
  --steps 1000 `
  --agents 512 `
  --seed 42 `
  --dump-every 50 `
  --dump-dir dumps `
  --dump-prefix batch_run `
  --report-html dumps\batch_run_report.html `
  --report-downsample 32 `
  --report-hist-bins 64
```

Result:
- CSV dumps in `dumps\`
- Offline HTML report `dumps\batch_run_report.html`

## Notes

- Use `--paper-mode` for metrics CSV.
- Add `--report-global-norm` for comparable heatmaps.
- Use `--dump-every 0` to disable dumps (no report).

