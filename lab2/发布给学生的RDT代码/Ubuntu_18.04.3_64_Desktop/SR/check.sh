#!/bin/sh

# 用法：
#     ./check.sh -x 可执行文件 -i 输入文件
usage() {
    echo "Usage: $0 -x {SR_PATH} -i {INPUT_FILE}"
}

while getopts ":x:i:" o; do
    case "${o}" in
        x)
            sr_path="$OPTARG"
            ;;
        i)
            input_file="$OPTARG"
            ;;
        *)
            usage
            ;;
    esac
done
shift $((OPTIND-1))

if [ -z "$sr_path" ] || [ -z "$input_file" ]; then
    usage
    exit 1
fi

sr_dir="$(dirname "$sr_path")"
sr="$(basename "$sr_path")"

output_file='output.txt'
result_file="$(realpath --relative-to="$sr_dir" 'result.txt')"

for i in $(seq 10); do
    echo "SR Test $i:"
    # 防止你在 SR 中对输入文件路径进行了硬编码
    (
        cd "$sr_dir" || exit 1
        "./$sr" > "$result_file" 2>&1
    )
    diff "$input_file" "$output_file"
done
