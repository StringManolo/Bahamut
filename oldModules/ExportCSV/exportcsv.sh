#!/usr/bin/env bash
# Name: CSV Exporter
# Description: Exports all collected data to CSV files - Slow AF module just as a bash module demo
# Type: output
# Stage: 3
# Consumes: *
# Storage: delete
# Install:
# InstallScope: global

printf '{"bmop":"1.0","module":"csv-exporter-optimized","pid":%s}\n' $$

OUTPUT_DIR="output"
mkdir -p "$OUTPUT_DIR"

declare -A file_handles 
declare -A counts       

TOTAL_ITEMS=0
IN_BATCH=0
BATCH_FORMAT=""

normalize_json() {
  local line="$1"
  line="${line//: /:}"
  line="${line//, /,}"
  echo "$line"
}

write_data() {
  local format="$1"
  local value="$2"

  if [[ -z "${file_handles[$format]}" ]]; then
    exec {fd}>>"$OUTPUT_DIR/${format}.csv"
    file_handles[$format]=$fd
    counts[$format]=0

    printf "%s\n" "$format" >&${fd}
  fi

  printf "%s\n" "$value" >&${file_handles[$format]}

  ((counts[$format]++))
  ((TOTAL_ITEMS++))
}

while IFS= read -r line; do
  [[ -z "$line" ]] && continue

    if [[ "$line" == *'"t":'*'"batch"'* ]]; then
      IN_BATCH=1
      normalized_line=$(normalize_json "$line")
      BATCH_FORMAT="${normalized_line#*'"f":"'}"
      BATCH_FORMAT="${BATCH_FORMAT%%'"'*}"
      continue
    fi

    if [[ "$line" == *'"t":'*'"batch_end"'* ]]; then
      IN_BATCH=0
      BATCH_FORMAT=""
      continue
    fi

    if [[ $IN_BATCH -eq 1 ]] && [[ -n "$BATCH_FORMAT" ]]; then
      write_data "$BATCH_FORMAT" "$line"
      continue
    fi

    if [[ "$line" == *'"t":'*'"d"'* ]]; then
      # Normalizar y extraer formato y valor
      normalized_line=$(normalize_json "$line")
      tmp_rest="${normalized_line#*'"f":"'}"
      tmp_format="${tmp_rest%%'"'*}"
      tmp_rest="${normalized_line#*'"v":"'}"
      tmp_value="${tmp_rest%%'"'*}"

      if [[ -n "$tmp_format" ]] && [[ -n "$tmp_value" ]]; then
        write_data "$tmp_format" "$tmp_value"
      fi
    fi
  done

for fd in "${file_handles[@]}"; do
  exec {fd}>&-
done

printf '{"t":"log","l":"info","m":"CSV export completed"}\n' >&2
printf '{"t":"log","l":"info","m":"Output directory: %s"}\n' "$OUTPUT_DIR" >&2

for format in "${!counts[@]}"; do
  printf '{"t":"log","l":"info","m":"  %s.csv: %s items"}\n' "$format" "${counts[$format]}" >&2
  printf '{"t":"d","f":"file","v":"%s/%s.csv"}\n' "$OUTPUT_DIR" "$format"
done

printf '{"t":"result","ok":true,"count":%s}\n' "$TOTAL_ITEMS"
