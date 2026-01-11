import { useState } from 'react';
import { Link } from 'react-router-dom';
import { authService } from '@/services/authService';
import { Button } from '@/components/ui/button';
import { Input } from '@/components/ui/input';
import { Card, CardContent, CardDescription, CardHeader, CardTitle, CardFooter } from '@/components/ui/card';
import { Label } from '@/components/ui/label';
import { AlertCircle, CheckCircle2 } from 'lucide-react';

export default function ForgotPassword() {
    const [email, setEmail] = useState('');
    const [isLoading, setIsLoading] = useState(false);
    const [submitted, setSubmitted] = useState(false);
    const [error, setError] = useState('');

    const handleSubmit = async (e: React.FormEvent) => {
        e.preventDefault();
        setIsLoading(true);
        setError('');

        try {
            await authService.forgotPassword(email);
            setSubmitted(true);
        } catch (err: any) {
            setError(err.response?.data?.error || "Failed to process request. Please try again.");
        } finally {
            setIsLoading(false);
        }
    };

    return (
        <div className="flex min-h-screen items-center justify-center bg-gray-50/50 p-4">
            <Card className="w-full max-w-md">
                <CardHeader className="space-y-1">
                    <div className="flex justify-center mb-4">
                        <div className="h-10 w-10 rounded-full bg-primary flex items-center justify-center">
                            <span className="text-primary-foreground font-bold text-xl">d</span>
                        </div>
                    </div>
                    <CardTitle className="text-2xl text-center">Reset Password</CardTitle>
                    <CardDescription className="text-center">
                        Enter your email address to receive a password reset link.
                    </CardDescription>
                </CardHeader>
                <CardContent>
                    {!submitted ? (
                        <form onSubmit={handleSubmit} className="space-y-4">
                            {error && (
                                <div className="flex items-center gap-2 rounded-md bg-destructive/15 p-3 text-sm text-destructive">
                                    <AlertCircle className="h-4 w-4" />
                                    <p>{error}</p>
                                </div>
                            )}
                            <div className="space-y-2">
                                <Label htmlFor="email">Email</Label>
                                <Input
                                    id="email"
                                    type="email"
                                    placeholder="m@example.com"
                                    value={email}
                                    onChange={(e) => setEmail(e.target.value)}
                                    required
                                />
                            </div>
                            <Button className="w-full" type="submit" disabled={isLoading}>
                                {isLoading ? "Sending..." : "Send Reset Link"}
                            </Button>
                        </form>
                    ) : (
                        <div className="flex flex-col items-center space-y-4 text-center py-4">
                            <CheckCircle2 className="h-12 w-12 text-green-500" />
                            <div className="space-y-2">
                                <h3 className="font-semibold text-lg">Check your inbox</h3>
                                <p className="text-sm text-muted-foreground">
                                    We have sent a password reset link to <strong>{email}</strong>.
                                </p>
                            </div>
                            <Button asChild variant="outline" className="w-full mt-4">
                                <Link to="/login">Back to Login</Link>
                            </Button>
                        </div>
                    )}
                </CardContent>
                {!submitted && (
                    <CardFooter className="flex justify-center">
                        <Link to="/login" className="text-sm text-muted-foreground hover:text-primary underline-offset-4 hover:underline">
                            Back to Login
                        </Link>
                    </CardFooter>
                )}
            </Card>
        </div>
    );
}
