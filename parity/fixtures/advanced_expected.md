# Advanced procedure smoke expectations

These are **StatDesk MVP** checks — not SPSS 30 goldens (no IBM license/binary fixtures).

| Command | Fixture input | Expect in HTML |
|---------|---------------|----------------|
| UNIANOVA | sample.csv score BY group WITH age | Parameter Estimates or Between-Subjects |
| FACTOR | age score | Component / Eigenvalue |
| QUICK CLUSTER | age score k=2 | Cluster Centers |
| LOGISTIC | sex ~ age score | Variables in the Equation |
| CTABLES | sex BY group | Custom Table |
| EXACT | sex BY group | Fisher's Exact Test |
| TREE | score BY age | Tree Table |

Tolerances: algorithms are documented approximations (power-iteration PCA, IRLS logistic, single-split tree). Do not claim numerical parity with SPSS.
