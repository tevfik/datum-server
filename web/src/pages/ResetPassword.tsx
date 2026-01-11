import { useState, useEffect } from 'react';
import { useSearchParams, Link, useNavigate } from 'react-router-dom';
import { authService } from '@/services/authService';
import { Button } from '@/components/ui/button';
import { Input } from '@/components/ui/input';
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from '@/components/ui/card';
import { Label } from '@/components/ui/label';
import { AlertCircle, CheckCircle2 } from 'lucide-react';

export default function ResetPassword() {
    const [searchParams] = useSearchParams();
    const token = searchParams.get('token');
    const navigate = useNavigate();

    const [password, setPassword] = useState('');
    const [confirmPassword, setConfirmPassword] = useState('');
    const [isLoading, setIsLoading] = useState(false);
    const [error, setError] = useState('');
    const [success, setSuccess] = useState(false);

    useEffect(() => {
        if (!token) {
            setError("Missing or invalid reset token.");
        }
    }, [token]);

    const handleSubmit = async (e: React.FormEvent) => {
        e.preventDefault();

        if (password !== confirmPassword) {
            setError("Passwords do not match");
            return;
        }

        if (password.length < 8) {
            setError("Password must be at least 8 characters");
            return;
        }

        if (!token) {
            setError("Invalid token");
            return;
        }

        setIsLoading(true);
        setError('');

        try {
            await authService.resetPassword(token, password);
            setSuccess(true);
            setTimeout(() => {
                navigate('/login');
            }, 3000);
        } catch (err: any) {
            setError(err.response?.data?.error || "Failed to reset password. Link may be expired.");
        } finally {
            setIsLoading(false);
        }
    };

    if (!token) {
        return (
            <div className="flex min-h-screen items-center justify-center bg-gray-50/50 p-4">
                <Card className="w-full max-w-md">
                    <CardHeader>
                        <CardTitle className="text-destructive flex items-center gap-2">
                            <AlertCircle className="h-5 w-5" /> Invalid Link
                        </CardTitle>
                        <CardDescription>
                            This password reset link is invalid or missing the required token.
                        </CardDescription>
                    </CardHeader>
                    <CardContent>
                        <Button asChild className="w-full">
                            <Link to="/login">Return to Login</Link>
                        </Button>
                    </CardContent>
                </Card>
            </div>
        )
    }

    return (
        <div className="flex min-h-screen items-center justify-center bg-gray-50/50 p-4">
            <Card className="w-full max-w-md">
                <CardHeader className="space-y-1">
                    <div className="flex justify-center mb-4">
                        <div className="h-10 w-10 rounded-full bg-primary flex items-center justify-center">
                            <span className="text-primary-foreground font-bold text-xl">d</span>
                        </div>
                    </div>
                    <CardTitle className="text-2xl text-center">Set New Password</CardTitle>
                    <CardDescription className="text-center">
                        Enter your new password below.
                    </CardDescription>
                </CardHeader>
                <CardContent>
                    {!success ? (
                        <form onSubmit={handleSubmit} className="space-y-4">
                            {error && (
                                <div className="flex items-center gap-2 rounded-md bg-destructive/15 p-3 text-sm text-destructive">
                                    <AlertCircle className="h-4 w-4" />
                                    <p>{error}</p>
                                </div>
                            )}
                            <div className="space-y-2">
                                <Label htmlFor="password">New Password</Label>
                                <Input
                                    id="password"
                                    type="password"
                                    value={password}
                                    onChange={(e) => setPassword(e.target.value)}
                                    placeholder="Min. 8 characters"
                                    required
                                />
                            </div>
                            <div className="space-y-2">
                                <Label htmlFor="confirmPassword">Confirm Password</Label>
                                <Input
                                    id="confirmPassword"
                                    type="password"
                                    value={confirmPassword}
                                    onChange={(e) => setConfirmPassword(e.target.value)}
                                    required
                                />
                            </div>
                            <Button className="w-full" type="submit" disabled={isLoading}>
                                {isLoading ? "Resetting..." : "Reset Password"}
                            </Button>
                        </form>
                    ) : (
                        <div className="flex flex-col items-center space-y-4 text-center py-4">
                            <CheckCircle2 className="h-12 w-12 text-green-500" />
                            <h3 className="font-semibold text-lg">Password Reset!</h3>
                            <p className="text-sm text-muted-foreground">
                                Your password has been successfully updated. Redirecting to login...
                            </p>
                        </div>
                    )}
                </CardContent>
            </Card>
        </div>
    );
}
