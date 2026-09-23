#!/usr/bin/env python3
"""
Benchmark do TB1 - SSC0903 Computação de Alto Desempenho.

O script deve ficar na raiz do projeto, no mesmo nível de:
    Makefile
    src/
    tests/

Ele:
1. compila fire_seq e fire_omp;
2. executa a versão sequencial e a paralela várias vezes;
3. usa o campo "tempo" produzido pelo próprio programa, conforme a especificação;
4. valida que a saída funcional é igual entre execuções;
5. calcula mediana, média e desvio-padrão;
6. calcula speedup e eficiência;
7. testa diferentes schedules via OMP_SCHEDULE;
8. salva os dados em CSV;
9. gera os gráficos do relatório.

Exemplo:
    python3 benchmark.py

Exemplo com parâmetros:
    python3 benchmark.py --input tests/entrada_carga_grande.txt \
        --threads 1,2,4,8 --repetitions 7

Dependência Python:
    pip install matplotlib
"""

from __future__ import annotations

import argparse
import csv
import math
import os
import re
import statistics
import subprocess
import sys
import tempfile
from pathlib import Path


# ---------------------------------------------------------------------------
# Configuração padrão
# ---------------------------------------------------------------------------

DEFAULT_INPUT = Path("tests/entrada_carga_grande.txt")
DEFAULT_THREADS = [1, 2, 4, 8]
DEFAULT_SCHEDULES = ["static", "dynamic,1", "dynamic,16", "guided"]
DEFAULT_REPETITIONS = 7
DEFAULT_WARMUPS = 1

OUTPUT_DIR = Path("benchmark_results")
GRAPH_DIR = OUTPUT_DIR / "graficos"
RAW_CSV = OUTPUT_DIR / "resultados_brutos.csv"
SUMMARY_CSV = OUTPUT_DIR / "resultados_resumo.csv"
VALIDATION_CSV = OUTPUT_DIR / "validacao.csv"


# Os campos abaixo são os resultados funcionais definidos pela especificação.
# "tempo" fica de fora porque pode variar entre execuções.
OUTPUT_FIELDS = [
    "passos",
    "nao_combustiveis",
    "intactas",
    "em_chamas",
    "queimadas",
    "contencao",
    "total_ignicoes",
    "pico_ignicoes",
    "percentual_queimado",
    "percentual_protegido",
    "checksum",
]


# ---------------------------------------------------------------------------
# Utilidades
# ---------------------------------------------------------------------------

def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Executa e analisa benchmarks do TB1 com C/OpenMP."
    )
    parser.add_argument(
        "--input",
        type=Path,
        default=DEFAULT_INPUT,
        help=f"Entrada usada no benchmark (padrão: {DEFAULT_INPUT})",
    )
    parser.add_argument(
        "--threads",
        default=",".join(map(str, DEFAULT_THREADS)),
        help="Threads separadas por vírgula (padrão: 1,2,4,8)",
    )
    parser.add_argument(
        "--schedules",
        default=",".join(DEFAULT_SCHEDULES),
        help="Schedules separados por vírgula",
    )
    parser.add_argument(
        "--repetitions",
        type=int,
        default=DEFAULT_REPETITIONS,
        help=f"Repetições por configuração (padrão: {DEFAULT_REPETITIONS})",
    )
    parser.add_argument(
        "--warmups",
        type=int,
        default=DEFAULT_WARMUPS,
        help=f"Execuções de aquecimento por configuração (padrão: {DEFAULT_WARMUPS})",
    )
    parser.add_argument(
        "--schedule-threads",
        type=int,
        default=None,
        help=(
            "Número de threads usado no experimento de schedules. "
            "Se omitido, usa a maior quantidade de threads testada."
        ),
    )
    parser.add_argument(
        "--no-build",
        action="store_true",
        help="Não executa 'make clean && make'.",
    )
    return parser.parse_args()


def parse_int_list(value: str) -> list[int]:
    values = []
    for item in value.split(","):
        item = item.strip()
        if not item:
            continue
        number = int(item)
        if number <= 0:
            raise ValueError("O número de threads deve ser positivo.")
        values.append(number)

    if not values:
        raise ValueError("A lista de threads não pode ser vazia.")

    return values


def ensure_project_root() -> Path:
    """
    Usa o diretório onde o script está localizado como raiz do projeto.
    """
    root = Path(__file__).resolve().parent

    required = [
        root / "Makefile",
        root / "src" / "fire_seq.c",
        root / "src" / "fire_omp.c",
    ]

    missing = [str(path) for path in required if not path.exists()]

    if missing:
        raise FileNotFoundError(
            "O benchmark.py deve estar na raiz do projeto. "
            "Arquivos ausentes:\n  - " + "\n  - ".join(missing)
        )

    return root


def compile_project(root: Path) -> None:
    print("\n=== Compilando o projeto ===")

    subprocess.run(
        ["make", "clean"],
        cwd=root,
        check=True,
    )

    subprocess.run(
        ["make", "all"],
        cwd=root,
        check=True,
    )

    print("Compilação concluída.\n")


def parse_program_output(stdout: str) -> dict[str, str]:
    """
    Extrai a saída padronizada da aplicação.

    Exemplo:
        passos: 37
        checksum: 123
        tempo: 1.274839
    """
    result: dict[str, str] = {}

    for line in stdout.splitlines():
        match = re.match(r"^\s*([^:]+):\s*(.*?)\s*$", line)
        if match:
            key = match.group(1).strip()
            value = match.group(2).strip()
            result[key] = value

    missing = [field for field in OUTPUT_FIELDS + ["tempo"] if field not in result]

    if missing:
        raise RuntimeError(
            "A saída do programa não contém os campos esperados: "
            + ", ".join(missing)
            + f"\n\nSaída recebida:\n{stdout}"
        )

    return result


def run_program(
    executable: str,
    input_path: Path,
    root: Path,
    threads: int | None = None,
    schedule: str | None = None,
) -> tuple[dict[str, str], str]:
    """
    Executa o programa e retorna:
        (campos_parsed, stdout_completo)
    """
    env = os.environ.copy()

    if executable == "fire_omp":
        env["OMP_DYNAMIC"] = "FALSE"

    if schedule is not None:
        # fire_omp.c usa schedule(runtime), portanto OMP_SCHEDULE
        # controla a política do laço.
        env["OMP_SCHEDULE"] = schedule

    if threads is not None and executable != "fire_omp":
        raise ValueError("O número de threads só pode ser definido para fire_omp.")

    with tempfile.TemporaryDirectory(prefix="fire_benchmark_") as temp_dir:
        actual_input = input_path
        if threads is not None:
            with input_path.open("r", encoding="utf-8") as file:
                header = file.readline()
                remainder = file.read()
            fields = header.split()
            if len(fields) != 6:
                raise ValueError("A primeira linha da entrada deve conter L C P T seed limiar.")
            fields[3] = str(threads)
            actual_input = Path(temp_dir) / input_path.name
            actual_input.write_text(" ".join(fields) + "\n" + remainder, encoding="utf-8")

        completed = subprocess.run(
            [str(root / executable), str(actual_input)],
            cwd=root,
            env=env,
            capture_output=True,
            text=True,
            check=False,
        )

    if completed.returncode != 0:
        raise RuntimeError(
            f"Execução de {executable} falhou "
            f"(código {completed.returncode}).\n"
            f"stdout:\n{completed.stdout}\n"
            f"stderr:\n{completed.stderr}"
        )

    return parse_program_output(completed.stdout), completed.stdout


def get_timing(output: dict[str, str]) -> float:
    try:
        return float(output["tempo"])
    except ValueError as exc:
        raise RuntimeError(
            f"Valor inválido para 'tempo': {output['tempo']!r}"
        ) from exc


def functional_signature(output: dict[str, str]) -> tuple[str, ...]:
    """
    Retorna somente os campos funcionais, excluindo 'tempo'.
    """
    return tuple(output[field] for field in OUTPUT_FIELDS)


def format_seconds(value: float) -> str:
    return f"{value:.6f}"


# ---------------------------------------------------------------------------
# Benchmark
# ---------------------------------------------------------------------------

def benchmark_configuration(
    executable: str,
    input_path: Path,
    root: Path,
    repetitions: int,
    warmups: int,
    threads: int | None = None,
    schedule: str | None = None,
) -> tuple[list[float], dict[str, str]]:
    """
    Executa uma configuração várias vezes.

    O tempo utilizado é o campo "tempo" impresso pelo próprio programa,
    e não o tempo externo de subprocess.run().
    """
    for _ in range(warmups):
        run_program(
            executable,
            input_path,
            root,
            threads=threads,
            schedule=schedule,
        )

    times: list[float] = []
    reference_output: dict[str, str] | None = None

    for _ in range(repetitions):
        output, _ = run_program(
            executable,
            input_path,
            root,
            threads=threads,
            schedule=schedule,
        )

        if reference_output is None:
            reference_output = output
        else:
            if functional_signature(output) != functional_signature(reference_output):
                raise RuntimeError(
                    f"A mesma configuração produziu resultados funcionais "
                    f"diferentes em execuções consecutivas.\n"
                    f"Executável: {executable}\n"
                    f"Threads: {threads}\n"
                    f"Schedule: {schedule}\n"
                )

        times.append(get_timing(output))

    assert reference_output is not None
    return times, reference_output


def statistics_for(times: list[float]) -> dict[str, float]:
    mean = statistics.mean(times)
    median = statistics.median(times)

    if len(times) > 1:
        stdev = statistics.stdev(times)
    else:
        stdev = 0.0

    return {
        "mean": mean,
        "median": median,
        "stdev": stdev,
        "min": min(times),
        "max": max(times),
    }


# ---------------------------------------------------------------------------
# Validação
# ---------------------------------------------------------------------------

def validate_against_sequential(
    input_path: Path,
    root: Path,
    threads: list[int],
    schedule: str,
) -> list[dict[str, str]]:
    """
    Executa uma vez a versão sequencial e compara os campos funcionais
    com a versão OpenMP para cada quantidade de threads.
    """
    print("\n=== Validação funcional ===")

    seq_output, _ = run_program(
        "fire_seq",
        input_path,
        root,
    )
    seq_signature = functional_signature(seq_output)

    rows: list[dict[str, str]] = []

    for thread_count in threads:
        omp_output, _ = run_program(
            "fire_omp",
            input_path,
            root,
            threads=thread_count,
            schedule=schedule,
        )

        same = functional_signature(omp_output) == seq_signature

        print(
            f"  {thread_count:>3} threads | "
            f"schedule={schedule:<11} | "
            f"{'OK' if same else 'ERRO'}"
        )

        if not same:
            print("\nSaída sequencial:")
            for field in OUTPUT_FIELDS:
                print(f"  {field}: {seq_output[field]}")

            print("\nSaída OpenMP:")
            for field in OUTPUT_FIELDS:
                print(f"  {field}: {omp_output[field]}")

            raise RuntimeError(
                "A versão paralela não produziu os mesmos resultados "
                "funcionais da versão sequencial."
            )

        rows.append(
            {
                "threads": str(thread_count),
                "schedule": schedule,
                "status": "OK",
                **{field: omp_output[field] for field in OUTPUT_FIELDS},
            }
        )

    return rows


# ---------------------------------------------------------------------------
# CSV
# ---------------------------------------------------------------------------

def save_raw_results(rows: list[dict[str, object]]) -> None:
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)

    fieldnames = [
        "experiment",
        "version",
        "threads",
        "schedule",
        "repetition",
        "time",
    ]

    with RAW_CSV.open("w", newline="", encoding="utf-8") as file:
        writer = csv.DictWriter(file, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)


def save_summary(rows: list[dict[str, object]]) -> None:
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)

    fieldnames = [
        "experiment",
        "version",
        "threads",
        "schedule",
        "mean",
        "median",
        "stdev",
        "min",
        "max",
        "speedup",
        "efficiency_percent",
    ]

    with SUMMARY_CSV.open("w", newline="", encoding="utf-8") as file:
        writer = csv.DictWriter(file, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)


def save_validation(rows: list[dict[str, str]]) -> None:
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)

    fieldnames = ["threads", "schedule", "status", *OUTPUT_FIELDS]

    with VALIDATION_CSV.open("w", newline="", encoding="utf-8") as file:
        writer = csv.DictWriter(file, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)


# ---------------------------------------------------------------------------
# Gráficos
# ---------------------------------------------------------------------------

def import_matplotlib():
    try:
        import matplotlib.pyplot as plt
    except ImportError:
        print(
            "\nERRO: matplotlib não está instalado.\n"
            "Instale com:\n"
            "    python3 -m pip install matplotlib\n"
        )
        sys.exit(1)

    return plt


def generate_graphs(summary_rows: list[dict[str, object]]) -> None:
    plt = import_matplotlib()

    GRAPH_DIR.mkdir(parents=True, exist_ok=True)

    thread_rows = [
        row
        for row in summary_rows
        if row["experiment"] == "threads"
        and row["version"] == "openmp"
    ]

    thread_rows.sort(key=lambda row: int(row["threads"]))

    if thread_rows:
        threads = [int(row["threads"]) for row in thread_rows]
        times = [float(row["median"]) for row in thread_rows]
        speedups = [float(row["speedup"]) for row in thread_rows]
        efficiencies = [float(row["efficiency_percent"]) for row in thread_rows]

        # Tempo x threads
        plt.figure(figsize=(8, 5))
        plt.plot(threads, times, marker="o")
        plt.xlabel("Número de threads")
        plt.ylabel("Tempo de execução (s)")
        plt.title("Tempo de execução em função do número de threads")
        plt.xticks(threads)
        plt.grid(True, alpha=0.3)
        plt.tight_layout()
        plt.savefig(GRAPH_DIR / "tempo_threads.png", dpi=300)
        plt.close()

        # Speedup
        plt.figure(figsize=(8, 5))
        plt.plot(threads, speedups, marker="o", label="Speedup medido")

        # Referência ideal: speedup = número de threads
        plt.plot(threads, threads, linestyle="--", label="Speedup ideal")

        plt.xlabel("Número de threads")
        plt.ylabel("Speedup")
        plt.title("Speedup em função do número de threads")
        plt.xticks(threads)
        plt.grid(True, alpha=0.3)
        plt.legend()
        plt.tight_layout()
        plt.savefig(GRAPH_DIR / "speedup.png", dpi=300)
        plt.close()

        # Eficiência
        plt.figure(figsize=(8, 5))
        plt.plot(threads, efficiencies, marker="o")
        plt.axhline(100, linestyle="--", label="Eficiência ideal")
        plt.xlabel("Número de threads")
        plt.ylabel("Eficiência (%)")
        plt.title("Eficiência em função do número de threads")
        plt.xticks(threads)
        plt.grid(True, alpha=0.3)
        plt.legend()
        plt.tight_layout()
        plt.savefig(GRAPH_DIR / "eficiencia.png", dpi=300)
        plt.close()

    schedule_rows = [
        row
        for row in summary_rows
        if row["experiment"] == "schedules"
    ]

    schedule_rows.sort(key=lambda row: str(row["schedule"]))

    if schedule_rows:
        schedules = [str(row["schedule"]) for row in schedule_rows]
        times = [float(row["median"]) for row in schedule_rows]

        plt.figure(figsize=(8, 5))
        bars = plt.bar(schedules, times)

        plt.xlabel("Schedule")
        plt.ylabel("Tempo de execução (s)")
        plt.title("Tempo de execução por política de escalonamento")

        for bar, time in zip(bars, times):
            plt.text(
                bar.get_x() + bar.get_width() / 2,
                bar.get_height(),
                f"{time:.4f}",
                ha="center",
                va="bottom",
            )

        plt.grid(axis="y", alpha=0.3)
        plt.tight_layout()
        plt.savefig(GRAPH_DIR / "schedules.png", dpi=300)
        plt.close()

    print(f"\nGráficos salvos em: {GRAPH_DIR}")


# ---------------------------------------------------------------------------
# Programa principal
# ---------------------------------------------------------------------------

def main() -> None:
    args = parse_args()
    root = ensure_project_root()

    input_path = Path(args.input)
    if not input_path.is_absolute():
        input_path = root / input_path

    if not input_path.exists():
        raise FileNotFoundError(f"Entrada não encontrada: {input_path}")

    threads = parse_int_list(args.threads)
    schedules = [item.strip() for item in args.schedules.split(",") if item.strip()]

    # O split acima separaria "dynamic,1" em duas partes. Para preservar
    # schedules com vírgula, usamos a lista padrão quando ela não foi alterada.
    if args.schedules == ",".join(DEFAULT_SCHEDULES):
        schedules = DEFAULT_SCHEDULES.copy()
    else:
        # Para customização, use ponto e vírgula:
        # --schedules "static;dynamic,1;guided"
        schedules = [item.strip() for item in args.schedules.split(";") if item.strip()]

    if args.repetitions <= 0:
        raise ValueError("--repetitions deve ser positivo.")

    if args.warmups < 0:
        raise ValueError("--warmups não pode ser negativo.")

    schedule_threads = args.schedule_threads or max(threads)

    if schedule_threads <= 0:
        raise ValueError("--schedule-threads deve ser positivo.")

    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)

    print("=== Benchmark TB1 - SSC0903 ===")
    print(f"Projeto:       {root}")
    print(f"Entrada:       {input_path}")
    print(f"Threads:       {threads}")
    print(f"Repetições:    {args.repetitions}")
    print(f"Aquecimentos:  {args.warmups}")
    print(f"Schedules:     {schedules}")
    print(f"Threads/sched: {schedule_threads}")

    if not args.no_build:
        compile_project(root)

    # -----------------------------------------------------------------------
    # Validação
    # -----------------------------------------------------------------------

    validation_rows = validate_against_sequential(
        input_path=input_path,
        root=root,
        threads=threads,
        schedule=schedules[0],
    )
    save_validation(validation_rows)

    # -----------------------------------------------------------------------
    # Benchmark
    # -----------------------------------------------------------------------

    raw_rows: list[dict[str, object]] = []
    summary_rows: list[dict[str, object]] = []

    print("\n=== Benchmark de threads ===")

    # Primeiro: baseline sequencial.
    seq_times, seq_output = benchmark_configuration(
        executable="fire_seq",
        input_path=input_path,
        root=root,
        repetitions=args.repetitions,
        warmups=args.warmups,
    )

    seq_stats = statistics_for(seq_times)
    seq_median = seq_stats["median"]

    print(
        f"  sequencial | mediana={seq_median:.6f}s "
        f"| média={seq_stats['mean']:.6f}s "
        f"| desvio={seq_stats['stdev']:.6f}s"
    )

    for i, time in enumerate(seq_times, start=1):
        raw_rows.append(
            {
                "experiment": "threads",
                "version": "sequential",
                "threads": 1,
                "schedule": "",
                "repetition": i,
                "time": time,
            }
        )

    summary_rows.append(
        {
            "experiment": "threads",
            "version": "sequential",
            "threads": 1,
            "schedule": "",
            "mean": seq_stats["mean"],
            "median": seq_stats["median"],
            "stdev": seq_stats["stdev"],
            "min": seq_stats["min"],
            "max": seq_stats["max"],
            "speedup": 1.0,
            "efficiency_percent": 100.0,
        }
    )

    # Paralelo com schedule static como baseline de threads.
    thread_schedule = schedules[0]

    for thread_count in threads:
        times, output = benchmark_configuration(
            executable="fire_omp",
            input_path=input_path,
            root=root,
            repetitions=args.repetitions,
            warmups=args.warmups,
            threads=thread_count,
            schedule=thread_schedule,
        )

        stats = statistics_for(times)
        speedup = seq_median / stats["median"]
        efficiency = speedup / thread_count * 100.0

        print(
            f"  {thread_count:>3} threads | "
            f"schedule={thread_schedule:<11} | "
            f"mediana={stats['median']:.6f}s | "
            f"speedup={speedup:.3f} | "
            f"eficiência={efficiency:.2f}%"
        )

        for i, time in enumerate(times, start=1):
            raw_rows.append(
                {
                    "experiment": "threads",
                    "version": "openmp",
                    "threads": thread_count,
                    "schedule": thread_schedule,
                    "repetition": i,
                    "time": time,
                }
            )

        summary_rows.append(
            {
                "experiment": "threads",
                "version": "openmp",
                "threads": thread_count,
                "schedule": thread_schedule,
                "mean": stats["mean"],
                "median": stats["median"],
                "stdev": stats["stdev"],
                "min": stats["min"],
                "max": stats["max"],
                "speedup": speedup,
                "efficiency_percent": efficiency,
            }
        )

    # -----------------------------------------------------------------------
    # Benchmark de schedules
    # -----------------------------------------------------------------------

    print("\n=== Benchmark de schedules ===")

    for schedule in schedules:
        times, output = benchmark_configuration(
            executable="fire_omp",
            input_path=input_path,
            root=root,
            repetitions=args.repetitions,
            warmups=args.warmups,
            threads=schedule_threads,
            schedule=schedule,
        )

        stats = statistics_for(times)

        print(
            f"  {schedule:<12} | "
            f"{schedule_threads:>3} threads | "
            f"mediana={stats['median']:.6f}s | "
            f"média={stats['mean']:.6f}s"
        )

        for i, time in enumerate(times, start=1):
            raw_rows.append(
                {
                    "experiment": "schedules",
                    "version": "openmp",
                    "threads": schedule_threads,
                    "schedule": schedule,
                    "repetition": i,
                    "time": time,
                }
            )

        summary_rows.append(
            {
                "experiment": "schedules",
                "version": "openmp",
                "threads": schedule_threads,
                "schedule": schedule,
                "mean": stats["mean"],
                "median": stats["median"],
                "stdev": stats["stdev"],
                "min": stats["min"],
                "max": stats["max"],
                "speedup": "",
                "efficiency_percent": "",
            }
        )

    save_raw_results(raw_rows)
    save_summary(summary_rows)
    generate_graphs(summary_rows)

    print("\n=== Arquivos gerados ===")
    print(f"Dados brutos:  {RAW_CSV}")
    print(f"Resumo:        {SUMMARY_CSV}")
    print(f"Validação:     {VALIDATION_CSV}")
    print(f"Gráficos:      {GRAPH_DIR}/")
    print("\nBenchmark concluído.")


if __name__ == "__main__":
    main()
