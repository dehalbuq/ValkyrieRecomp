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
