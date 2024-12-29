set xlabel "x-coordinate (m)"
set ylabel "y-coordinate (m)"
set cblabel "SNR (dB)"
set cblabel offset 3
unset key
set terminal pdf
set output "nr-rem-DL_UE_COVERAGE-snr.pdf"
set size ratio -1
set cbrange [-5:30]
set xrange [-40:80]
set yrange [-70:50]
set xtics font "Times New Roman,17"
set ytics font "Times New Roman,17"
set cbtics font "Times New Roman,17"
set xlabel font "Times New Roman,17"
set ylabel font "Times New Roman,17"
set cblabel font "Times New Roman,17"
plot "nr-rem-DL_UE_COVERAGE.out" using ($1):($2):($4) with image
set xlabel "x-coordinate (m)"
set ylabel "y-coordinate (m)"
set cblabel "SINR (dB)"
set cblabel offset 3
unset key
set terminal pdf
set output "nr-rem-DL_UE_COVERAGE-sinr.pdf"
set size ratio -1
set cbrange [-5:30]
set xrange [-40:80]
set yrange [-70:50]
set xtics font "Times New Roman,17"
set ytics font "Times New Roman,17"
set cbtics font "Times New Roman,17"
set xlabel font "Times New Roman,17"
set ylabel font "Times New Roman,17"
set cblabel font "Times New Roman,17"
plot "nr-rem-DL_UE_COVERAGE.out" using ($1):($2):($5) with image
set xlabel "x-coordinate (m)"
set ylabel "y-coordinate (m)"
set cblabel "IPSD (dBm)"
set cblabel offset 3
unset key
set terminal pdf
set output "nr-rem-DL_UE_COVERAGE-ipsd.pdf"
set size ratio -1
set cbrange [-100:-20]
set xrange [-40:80]
set yrange [-70:50]
set xtics font "Times New Roman,17"
set ytics font "Times New Roman,17"
set cbtics font "Times New Roman,17"
set xlabel font "Times New Roman,17"
set ylabel font "Times New Roman,17"
set cblabel font "Times New Roman,17"
plot "nr-rem-DL_UE_COVERAGE.out" using ($1):($2):($6) with image
set xlabel "x-coordinate (m)"
set ylabel "y-coordinate (m)"
set cblabel "SIR (dB)"
set cblabel offset 3
unset key
set terminal pdf
set output "nr-rem-DL_UE_COVERAGE-sir.pdf"
set size ratio -1
set cbrange [-5:30]
set xrange [-40:80]
set yrange [-70:50]
set xtics font "Times New Roman,17"
set ytics font "Times New Roman,17"
set cbtics font "Times New Roman,17"
set xlabel font "Times New Roman,17"
set ylabel font "Times New Roman,17"
set cblabel font "Times New Roman,17"
plot "nr-rem-DL_UE_COVERAGE.out" using ($1):($2):($7) with image
