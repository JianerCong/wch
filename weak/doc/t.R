library(tidyverse)
library(tikzDevice)

N <- 5 #number of

l <- str_c('第',1:N,'批基准测试')

src <- list(
  list(mod = '存储模块 （读取）', mean = 8.5, sd = 1.5),
  list(mod = '存储模块 （存入）', mean = 10, sd = 1),
  list(mod = '执行模块', mean = 8.2, sd = 0.5)
)

df <- tibble(mod = src[[1]]$mod,
             tps = rnorm(n=N, mean=src[[1]]$mean, sd=src[[1]]$sd),
             id = l)
for (i in seq_along(src)){
  if (i > 1){
    df_tmp <- tibble(mod = src[[i]]$mod, tps = rnorm(n=N,
                                                   mean=src[[i]]$mean,
                                                   sd=src[[i]]$sd),
                     id = l
                     )
    df <- union(df,df_tmp)
  }

  df <- arrange(df,mod)
}


## --------------------------------------------------
## The individual value
rows_str <- ''
for (i in seq(N)){
  lab <- l[[i]]
  tps <- filter(df,id==lab)$tps
  s <- sprintf(
    paste0(
      '\n%s ',
      str_dup('& %.2f',3),
      '\\\\'
    )
  , lab,
               tps[[1]], tps[[2]], tps[[3]])
  rows_str <- str_c(rows_str,
                    s)
}

## --------------------------------------------------
## The mean and variance
df1 <- df %>% group_by(mod) %>%
  summarise(mean = mean(tps), var=var(tps)) %>%
  arrange(mod)

row_mean <- sprintf(paste0(
  '\n样本平均值 sample mean $\\mu$',
  str_dup('& %.2f',3),
  '\\\\')
 ,df1$mean[[1]]
 ,df1$mean[[2]]
 ,df1$mean[[3]]
  )

row_var <- sprintf(paste0(
  '\n样本方差 sample variance $\\sigma^{2}$',
  str_dup('& %.2f',3),
  '\\\\')
 ,df1$var[[1]]
 ,df1$var[[2]]
 ,df1$var[[3]]
  )

## Write to file result-table.tex
o <- str_c(
## '\\begin{tabularx}{0.8\\textwidth} {
##       | >{\\raggedright\\arraybackslash}X
##       | >{\\centering\\arraybackslash}X
##       | >{\\centering\\arraybackslash}X
##       | >{\\centering\\arraybackslash}X  |}
## \\hline
## ',
  '& 存储模块（存入）& 存储模块(读取) & 执行模块  \\\\ \\hline',
rows_str,'\\hline',
row_mean,
row_var,'\\hline'
)
cat(o,file='tex/result-table.tex')


library(colorspace)
p <- ggplot(df,aes(x=mod,y=tps,fill=as.character(id))) +
  geom_col(position='dodge',width=0.5) +
  geom_hline(yintercept = 7)+
  coord_flip() +
  labs(x=NULL,y='Transaction Per Seconds TPS（万）',fill='基准测试') +
  ## theme(legend.position = "none")+
  scale_fill_manual(
    values = sequential_hcl(
      N,
      h = 0,
      c = c(100,0),
      l = 65
    )
  )+
  annotate(
    geom = 'text', x = 1.5, y = 7, label = '7万TPS基准线',
    hjust = -0.1, vjust = 1, size = 4
  )

exportPlot <- function(texName,p,w,h){
  tikz(texName,
       width = w,
       height = h
       )
  print(p)
  dev.off()
}

options(tikzDefaultEngine='luatex')
options(tikzDocumentDeclaration='\\documentclass[dvipsnames]{ctexart}')
exportPlot("tex/p1.tex", p,7,4)
