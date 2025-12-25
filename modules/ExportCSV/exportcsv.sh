#!/usr/bin/env bash
# Name: CSV Exporter
# Description: Exports all collected data to CSV files
# Type: output
# Stage: 3
# Consumes: *
# Install:
# InstallScope: global

echo '{"bmop":"1.0","module":"csv-exporter","pid":'$$'}'

OUTPUT_DIR="output"
mkdir -p "$OUTPUT_DIR"

declare -A DATA_COUNTS
declare -A FILES_CREATED

TOTAL_ITEMS=0
IN_BATCH=0
BATCH_FORMAT=""

while IFS= read -r line; do
  [[ -z "$line" ]] && continue

  if echo "$line" | grep -q '"t":"batch"'; then
    IN_BATCH=1
    BATCH_FORMAT=$(echo "$line" | sed -n 's/.*"f":"\([^"]*\)".*/\1/p')
    
    if [[ -z "${FILES_CREATED[$BATCH_FORMAT]}" ]]; then
      echo "$BATCH_FORMAT" > "$OUTPUT_DIR/${BATCH_FORMAT}.csv"
      FILES_CREATED[$BATCH_FORMAT]=1
      DATA_COUNTS[$BATCH_FORMAT]=0
    fi
    continue
  fi

  if echo "$line" | grep -q '"t":"batch_end"'; then
    IN_BATCH=0
    BATCH_FORMAT=""
    continue
  fi

  if [[ $IN_BATCH -eq 1 ]] && [[ -n "$BATCH_FORMAT" ]]; then
    echo "$line" >> "$OUTPUT_DIR/${BATCH_FORMAT}.csv"
    ((DATA_COUNTS[$BATCH_FORMAT]++))
    ((TOTAL_ITEMS++))
    continue
  fi

  if echo "$line" | grep -q '"t":"d"'; then
    FORMAT=$(echo "$line" | sed -n 's/.*"f":"\([^"]*\)".*/\1/p')
    VALUE=$(echo "$line" | sed -n 's/.*"v":"\([^"]*\)".*/\1/p')

    if [[ -n "$FORMAT" ]] && [[ -n "$VALUE" ]]; then
      if [[ -z "${FILES_CREATED[$FORMAT]}" ]]; then
        echo "$FORMAT" > "$OUTPUT_DIR/${FORMAT}.csv"
        FILES_CREATED[$FORMAT]=1
        DATA_COUNTS[$FORMAT]=0
      fi

      echo "$VALUE" >> "$OUTPUT_DIR/${FORMAT}.csv"
      ((DATA_COUNTS[$FORMAT]++))
      ((TOTAL_ITEMS++))
    fi
  fi
done

echo "{\"t\":\"log\",\"l\":\"info\",\"m\":\"CSV export completed\"}" >&2
echo "{\"t\":\"log\",\"l\":\"info\",\"m\":\"Output directory: $OUTPUT_DIR\"}" >&2

for format in "${!DATA_COUNTS[@]}"; do
  count=${DATA_COUNTS[$format]}
  echo "{\"t\":\"log\",\"l\":\"info\",\"m\":\"  ${format}.csv: $count items\"}" >&2
  echo "{\"t\":\"d\",\"f\":\"file\",\"v\":\"$OUTPUT_DIR/${format}.csv\"}"
done

echo "{\"t\":\"result\",\"ok\":true,\"count\":$TOTAL_ITEMS}"
