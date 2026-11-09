# Laboratório ARM64 com base no dArkOSRE real (Fases 1–33)

Perfil coletado no portátil (somente metadados): Debian 13, glibc 2.41, 4 Cortex-A35 a até 1,512 GHz, 916800 KiB RAM utilizável, zero swap, kernel 4.4.189, SDL 2.32.x, Mali-G31 Bifrost GBM, áudio ALSA.

GitHub Actions constrói um contêiner Debian 13 ARM64 com o código-fonte do bootstrap, executa 29 verificações sintéticas JNI/Bionic/ELF, verifica o ciclo de vida do ELF de teste e mede pico de memória por etapa. O contêiner executa com limite de **4 CPUs**, **916800 KiB de RAM**, **sem swap**, rede desativada e sistema de arquivos somente leitura. O workflow original das fases anteriores continua rodando independentemente.

O perfil original enviado pelo usuário NÃO foi publicado: apenas especificações genéricas do ambiente foram incorporadas. Nenhum jogo, APK, asset, biblioteca comercial ou driver proprietário vai ao CI.

Limitações: Debian 13 e glibc ficam próximos do R36S, mas o contêiner usa o kernel do host, CPU de servidor e Mesa llvmpipe por software. O teste não representa Mali-G31, drivers GBM do dispositivo, timings, FPS ou compatibilidade exata com o kernel 4.4.189. Use o console apenas nas versões que precisam de validação de hardware.

O nome do arquivo do perfil informa 04/09/2025, enquanto o kernel reporta compilação de 13/12/2025: o relógio do console parece desajustado e não é uma referência temporal confiável.
