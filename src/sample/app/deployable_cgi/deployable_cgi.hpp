#ifndef __CXX_SAMPLE_CGI_APPLICATION_HPP
#define __CXX_SAMPLE_CGI_APPLICATION_HPP

class CCgiSampleApplication : public ncbi::CCgiApplication
{
public:
    virtual void Init(void);
    virtual int  ProcessRequest(ncbi::CCgiContext& ctx);

private:
    // These 2 functions just demonstrate the use of cmd-line argument parsing
    // mechanism in CGI application -- for the processing of both cmd-line
    // arguments and HTTP entries
    void x_SetupArgs(void);
    void x_LookAtArgs(void);
    const std::string GetCdVersion() const;
    int ProcessPrintEnvironment(ncbi::CCgiContext& ctx);
};


#endif /* __CXX_SAMPLE_CGI_APPLICATION_HPP */
