* Sample syntax for StatDesk smoke / differential runs (Base + advanced MVP).
FREQUENCIES VARIABLES=sex group.
DESCRIPTIVES VARIABLES=age score.
CROSSTABS /TABLES=sex BY group /STATISTICS=CHISQ.
T-TEST /TESTVAL=80 /VARIABLES=score.
CORRELATIONS /VARIABLES=age score.
UNIANOVA score BY group WITH age.
FACTOR /VARIABLES age score /EXTRACTION PC.
QUICK CLUSTER age score /CRITERIA=CLUSTER(2).
LOGISTIC REGRESSION VARIABLES=sex /METHOD=ENTER age score.
CTABLES /TABLE sex BY group.
TREE score BY age.
EXACT TABLES=sex BY group.
COMPUTE total = age + score.
EXECUTE.
