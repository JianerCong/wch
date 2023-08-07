library(tidyverse)

N <- 200 #number of

src <- list(
  list(mod = '存储模块 （读取）', mean = 11, sd = 0.5),
  list(mod = '存储模块 （存入）', mean = 10, sd = 0.5),
  list(mod = '执行', mean = 12, sd = 1)
)

df <- tibble(mod = src[[1]]$mod, x = rnorm(n=N,
                                               mean=src[[1]]$mean,
                                               sd=src[[1]]$sd))
for (i in seq_along(src)){
  if (i > 1){
    df_tmp <- tibble(mod = src[[i]]$mod, x = rnorm(n=N,
                                                   mean=src[[i]]$mean,
                                                   sd=src[[i]]$sd))
    df <- union(df,df_tmp)
  }
}

write_csv(df, "data/tps.csv")
df <- read_csv("data/tps.csv",
               col_types = cols(mod = col_character(), x = col_double())
               )

library(colorspace)


p <- ggplot(df,aes(x=x)) +
  geom_histogram(bins=40) +
  ## geom_freqpoly() +
  facet_wrap(~mod,ncol=1) +
  annotate(
    geom = 'text', x = 7, y =N/1.6, label = '7万TPS基准线',
    hjust = -0.1, vjust = 1, size = 4, color=munsell::mnsl("5B 5/6"))+
  xlab('每秒交易处理数 Transaction Per Second (TPS)（单位：万）') +
  ylab('测试结果样本统计累积 (单位： 次)')+
  geom_vline(xintercept = 7)+
  lims(x=c(0,1.2*max(df$x)),y=c(0,N/1.5))
p


library(tikzDevice)
exportPlot <- function(texName,p,w,h){
  tikz(texName,
       width = w,
       height = h
       )
  print(p)
  dev.off()
}
## exportPlot("tex/p1.tex", p, 7,5)
