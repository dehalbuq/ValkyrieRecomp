# Laboratório ARM64 com base no dArkOSRE real (Fases 1–33)

Perfil coletado no portátil (somente metadados): Debian 13, glibc 2.41, 4 Cortex-A35 a até 1,512 GHz, 916800 KiB RAM utilizável, zero swap, kernel 4.4.189, SDL 2.32.x, Mali-G31 Bifrost GBM, áudio ALSA.

GitHub Actions constrói um contêiner Debian 13 ARM64 com o código-fonte do bootstrap, executa 29 verificações sintéticas JNI/Bionic/ELF, verifica o ciclo de vida do ELF de teste e mede pico de memória por etapa. O contêiner executa com limite de **4 CPUs**, **916800 KiB de RAM**, **sem swap**, rede desativada e sistema de arquivos somente leitura. O workflow original das fases anteriores continua rodando independentemente.

O perfil original enviado pelo usuário NÃO foi publicado: apenas especificações genéricas do ambiente foram incorporadas. Nenhum jogo, APK, asset, biblioteca comercial ou driver proprietário vai ao CI.

Limitações: Debian 13 e glibc ficam próximos do R36S, mas o contêiner usa o kernel do host, CPU de servidor e Mesa llvmpipe por software. O teste não representa Mali-G31, drivers GBM do dispositivo, timings, FPS ou compatibilidade exata com o kernel 4.4.189. Use o console apenas nas versões que precisam de validação de hardware.

O nome do arquivo do perfil informa 04/09/2025, enquanto o kernel reporta compilação de 13/12/2025: o relógio do console parece desajustado e não é uma referência temporal confiável.

## Fase 34 — integração com biblioteca JNI sintética

O novo conjunto cloud/phase34_host.c e cloud/phase34_guest.c compila um ELF AArch64 sintético contra o cabeçalho JNI padrão (OpenJDK jni.h), carrega uma biblioteca criada apenas para testes, invoca JNI_OnLoad, valida RegisterNatives para dois callbacks de teste, chama nativeInit/nativeRender, exercita GetEnv e attach/detach por thread e rejeita nomes e assinaturas desconhecidos.

O log phase34.log e a tabela lenneth-jni-integration34.tsv são adicionados ao artefato Debian13. Os nomes dos métodos da biblioteca são fixtures, não assinaturas verificadas do jogo. Não há execução de arquivos comerciais nem uma implementação ART completa.

**Próximo bloqueio:** verificar assinaturas Java e serviços reais do APK (classes.dex e RegisterNatives), depois integrar assets e ciclo de vida Cocos2d ao carregador.


## Fase 35 — auditor local de classes.dex e JNI

O script `cloud/dex_audit35.py` lê no computador os arquivos `classes*.dex` de um APK ou um DEX individual, compara assinaturas Java/JNI e identifica métodos efetivamente declarados, métodos com `ACC_NATIVE` e meras referências a métodos externos. O relatório TSV contém apenas metadados, sem copiar ou enviar APK, arquivos do jogo, assets ou código executável. Nenhum upload automático acontece. Verifica cabeçalho, SHA1, Adler32, integridade dos índices e arquivos ZIP suspeitos.

**Uso local opcional:** `python3 dex_audit35.py /caminho/jogo.apk --output assinaturas-jni.tsv`. Não coloque o APK original no repositório público. Revise o TSV antes de compartilhar. O CI da Fase 35 usa apenas um DEX sintético criado por nosso código e **não comprova assinaturas do APK comercial**. O relatório da nuvem marca `EXTERNAL_APK=NOT_PROVIDED`. Nenhum teste no R36S é necessário nesta fase.

O próximo passo para integrar a versão Android 1.0.7 é confrontar as assinaturas do DEX verdadeiro com a lista de símbolos JNI da biblioteca original, sem subir nenhum arquivo comercial ao CI público.


## Fase36: cruzar JNI DEX e exports ELF (metadados locais)

Use o programa `cloud/jni_crosscheck36.py` com o arquivo `assinaturas-jni.tsv` extraído pelo auditor35 e o inventário `lenneth-jni-demand31.tsv` ou o ZIP original de diagnósticos R36S. O programa **não acessa código/arquivos binários comerciais** e não faz upload automático.

Exemplo: `python3 jni_crosscheck36.py --dex assinaturas-jni.tsv --inventory lenneth-r36s-diagnostics.zip --output fase36_cruzamento.tsv`.

Verifica os nomes JNI curtos e longos com a codificação JNI de classes/métodos (incluindo nomes Unicode e underscores). Distingue correspondências exatas, ambiguidade de sobrecargas e declarações nativas sem símbolo `Java_...` nesse módulo. Métodos nativos sem export podem usar `RegisterNatives`, outras bibliotecas, ou não possuir implementação: **não é um veredito**. O relatório da nuvem utiliza SOMENTE fixtures sintéticos, não contém dados do APK real.

Um cruzamento local com os metadados fornecidos mostrou 55 declarações `ACC_NATIVE` no APK, 29 exports `Java_...` na libMyGame.so, 29 correspondências e 26 declarações não exportadas por esse módulo. JNI_OnLoad também está exportado, mas a execução comercial segue bloqueada.

## Fase 37 — ciclo de vida JNI usando metadados conferidos

Os seis nomes de símbolos e descritores de `nativeSetContext`, `nativeInit`, `nativeRender`, `nativeOnPause`, `nativeOnResume` e `nativeOnSurfaceChanged` constam em `phase37_contract.tsv`. Eles foram conferidos por leitura offline das declarações nativas DEX e inventário ELF (`DEX35_ELF31_CONFIRMED`). Isso verifica a ABI nominal, **não** a implementação ou funcionalidade do jogo.

O CI compila **uma nova biblioteca ELF ARM64 sintética**, com nomes de exportações equivalentes aos do jogo, e carrega exclusivamente essa fixture. Valida a sequência de `JNI_OnLoad` 1.6 de teste, Context e AssetManager de teste, referências globais, inicialização 640×480, render, pausa, retomada, mudança de superfície 320×240 e limpeza dos dois handles. Testes negativos verificam ordem incorreta, AssetManager de tipo errado, contexto duplicado, tamanho inválido e render durante pausa.

O relatório `lenneth-jni-lifecycle37.tsv` diferencia claramente mocks aprovados e bloqueios reais. **Nenhum construtor comercial, JNI_OnLoad real, motor Cocos2d original ou asset do jogo é executado no CI público.** O ambiente não representa o kernel 4.4.189, a GPU Mali nem desempenho real do R36S. As regressões anteriores continuam em workflows independentes.
