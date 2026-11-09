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


## Fase 38 — JNI Context → NDK AAssetManager → leitura segura

O ELF ARM64 **sintético** phase38_guest.c usa as assinaturas públicas de JNI identificadas no APK e importa os símbolos NDK AAssetManager_fromJava, AAssetManager_open, AAsset_read, AAsset_seek64, AAsset_getLength64 e AAsset_close. O phase38_host.c fornece esses símbolos apenas para um diretório privado de teste, sem APK, assets ou motor comercial.

Um arquivo de teste original deste laboratório, config/boot.txt com o marcador PHASE38_BOOT_READY, só é gerado após testar a falha controlada de inicialização sem assets. O guest então tenta abrir, ler em duas partes, consultar comprimento, reposicionar, fechar e iniciar um ciclo render/pausa/retomada sintético. O acesso é limitado por openat com diretórios validados, O_NOFOLLOW, rejeição de symlinks, .., caminhos absolutos, segmentos vazios e caracteres inesperados; arquivo máximo de 1 MiB e limite de descritores abertos. O teste valida fechamento de todos os handles e liberação de ambas as global refs do Context/AssetManager.

Resultados em lenneth-asset-bridge38.tsv. O log distingue as verificações dos *bloqueios*: dados reais do APK, ART, JNI comercial, GPU Mali, driver/kernel dArkOSRE e motor Cocos2d real continuam sem validação ou bloqueados. Nenhum arquivo comercial é colocado no GitHub, nenhum código do jogo é executado. A regressão das Fases 1–37 permanece separada.


## Fase39: mapa real de dependências ELF ARM64 — leitura local, sem executar o jogo

O script Python 3 sem dependências externas \`cloud/elf_dependencies39.py\` analisa em modo estritamente *read-only* arquivos ELF64 AArch64 (arquivos \`libMyGame.so\` e \`libc++_shared.so\` do APK próprio, **apenas localmente**). Extrai \`SHT_DYNSYM\`, importações, exportações, \`DT_NEEDED\` e \`DT_SONAME\`, com validações de limites, índice, tamanho, arquitetura e tipo. Resulta no relatório \`lenneth-elf-dependencies39.tsv\`.

Uso local (não precisa do R36S, não faz upload): \`py elf_dependencies39.py --modules .\libMyGame.so .\libc++_shared.so --output .\dependencias39.tsv\`. Revise o relatório antes de compartilhá-lo. **Não coloque as bibliotecas comerciais no GitHub.**

Classificação conservadora: \`CROSS_ELF_CANDIDATE\` (símbolo coincidente entre bibliotecas fornecidas, NÃO prova relocação/call ABI); \`PROVIDER_NOT_ESTABLISHED\` (a fonte não foi identificada entre os arquivos fornecidos, **NÃO** significa que faltou do Android); \`WEAK_OPTIONAL\` (import weak); \`EXTERNAL_NOT_VERIFIED\` (biblioteca \`DT_NEEDED\` ausente da seleção local). Nenhum import de sistema é falsamente marcado como resolvido.

O CI roda com **duas bibliotecas ARM64 sintéticas**, uma delas importando símbolo disponibilizado, símbolo de propósito sem provedor e símbolo fraco opcional. Os testes recusam ELF malformado e verificam export/DT_NEEDED. O arquivo comercial original permanece totalmente fora do runner.

**Bloqueios ainda reais:** detalhes de relocação/versão de símbolos, Bionic Android versus glibc Linux, serviços Android/ART/Cocos2d reais, assets reais, Mali-G31 e execução do \`JNI_OnLoad\` original. A Fase39 gera o diagnóstico necessário para priorizar compatibilidade, sem alegar que o jogo já inicializa.


## Fase 40 — diagnóstico real, priorização e bloqueio fail-closed

O utilitário \`cloud/startup_gate40.py\` lê **somente metadados** dos três arquivos TSV conhecidos em um ZIP de diagnósticos do R36S (ou diretório local): \`lenneth-startup-gates.tsv\`, \`lenneth-phase25-startup-plan.tsv\` e \`lenneth-jni-demand31.tsv\`. Ele compara o inventário \`DT_NEEDED\`, o total de construtores com slots de relocação, as contagens JNI e os bloqueios do runtime e gera \`lenneth-startup-priorities40.tsv\` e \`lenneth-startup-summary40.txt\`.

Uso local: \`py startup_gate40.py --diagnostics .\lenneth-r36s-diagnostics.zip --output .\prioridades40.tsv --summary .\resumo40.txt\`.

Um diagnóstico R36S fornecido pelo usuário indicou 195 slots de construtor de \`libMyGame.so\`, um de \`libc++_shared.so\`, 29 exports \`Java_\`, 96 imports GL, necessidade de GLES1 e dez \`DT_NEEDED\` no módulo do jogo. Esses dados são metadados de inspeção: **nenhum construtor original foi executado**. Uma lista de prioridades local categorizou 18 ações (8 P0, 9 P1 e 1 P2), com barreiras em Bionic, ART/JNI, Cocos2d, assets, transições W^X/RELRO, áudio OpenSL e renderização Mali-G31.

A execução CI **não recebe** o ZIP real e usa um fixture com contagens inventadas para testar o verificador e seus casos negativos. A aprovação do audit é rigorosamente distinta de uma autorização de execução: \`GAME_BOOT_READY=NO\`, \`COMMERCIAL_ENGINE_EXECUTED=NO\`, e qualquer discrepância nos gates ou contagens bloqueia o teste. O relatório não demonstra compatibilidade real da Bionic/ART; não mapeia e não executa \`libMyGame.so\`.


## Fase 41 — inicializacao unica de objetos static em C++11 ARM64

O modulo src/cxx_guard41.c implementa um **candidato isolado** para o ABI Itanium C++: operacoes cxa_guard_acquire/release/abort sobre o primeiro byte de um guard de 64 bits, com sincronizacao por mutex e condvar da glibc hospedeira, tabela limitada a 256 guards, detecao de recursao, espera entre threads, abort e nova tentativa. Ha um autoteste no executavel: lenneth-probe --cxx-guard41-selftest.

A suite da nuvem compila uma biblioteca sintetica C++11 separada com uma funcao contendo um objeto static local inicializado dinamicamente. O proprio compilador emite chamadas __cxa_guard_*. O GNU linker redireciona as chamadas para nomes exclusivos __wrap___cxa_guard_* providos pelo host experimental, sem substituir os simbolos do runtime C++ do servidor. Um teste de oito threads valida valor unico e uma unica invocacao de inicializacao; o teste do guard valida espera, recursao, abort e reentrada.

Esses testes exercitam apenas codigo original do laboratorio, executado em Linux AArch64 glibc 2.41. **Nao executam os 196 construtores originais, nao provam o layout de guard da versao Bionic do jogo e nao tornam o motor comercial executavel.** O adapter NAO foi adicionado ao provedor de simbolos da libMyGame.so: a integracao depende da verificacao dos imports reais, TLS e ABI Android. As tabelas phase41_guard.tsv e os logs do CI declaram esse limite explicitamente. R36S so sera necessario para verificacoes que nao podem ser reproduzidas por esse laboratorio.
